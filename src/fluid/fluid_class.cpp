
#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/display.h>
#include <parasol/modules/fluid.h>

extern "C" {
 #include "lua.h"
 #include "lualib.h"
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

static ERROR run_script(objScript *);
static ERROR stack_args(lua_State *, OBJECTID, const FunctionField *, BYTE *);
static ERROR save_binary(objScript *, OBJECTID);

INLINE CSTRING check_bom(CSTRING Value)
{
   if (((char)Value[0] IS 0xef) and ((char)Value[1] IS 0xbb) and ((char)Value[2] IS 0xbf)) Value += 3; // UTF-8 BOM
   else if (((char)Value[0] IS 0xfe) and ((char)Value[1] IS 0xff)) Value += 2; // UTF-16 BOM big endian
   else if (((char)Value[0] IS 0xff) and ((char)Value[1] IS 0xfe)) Value += 2; // UTF-16 BOM little endian
   return Value;
}

// Dump the variables of any global table

static ERROR register_interfaces(objScript *) __attribute__ ((unused));
static void dump_global_table(objScript *, STRING Global) __attribute__ ((unused));

static void dump_global_table(objScript *Self, STRING Global)
{
   parasol::Log log("print_env");
   lua_State *lua = ((prvFluid *)Self->Head.ChildPrivate)->Lua;
   lua_getglobal(lua, Global);
   if (lua_istable(lua, -1) ) {
      lua_pushnil(lua);
      while (lua_next(lua, -2) != 0) {
         LONG type = lua_type(lua, -2);
         log.msg("%s = %s", lua_tostring(lua, -2), lua_typename(lua, type));
         lua_pop(lua, 1);
      }
   }
}

//****************************************************************************

static ERROR GET_Procedures(objScript *, STRING **, LONG *);

static const FieldArray clFields[] = {
   { "Procedures", FDF_VIRTUAL|FDF_ARRAY|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)GET_Procedures, NULL },
   END_FIELD
};

//****************************************************************************

static ERROR FLUID_ActionNotify(objScript *, struct acActionNotify *);
static ERROR FLUID_Activate(objScript *, APTR);
static ERROR FLUID_DataFeed(objScript *, struct acDataFeed *);
static ERROR FLUID_Free(objScript *, APTR);
static ERROR FLUID_Init(objScript *, APTR);
static ERROR FLUID_SaveToObject(objScript *, struct acSaveToObject *);

static const ActionArray clActions[] = {
   { AC_ActionNotify, (APTR)FLUID_ActionNotify },
   { AC_Activate,     (APTR)FLUID_Activate },
   { AC_DataFeed,     (APTR)FLUID_DataFeed },
   { AC_Free,         (APTR)FLUID_Free },
   { AC_Init,         (APTR)FLUID_Init },
   { AC_SaveToObject, (APTR)FLUID_SaveToObject },
   { 0, NULL }
};

//****************************************************************************

static ERROR FLUID_GetProcedureID(objScript *, struct scGetProcedureID *);
static ERROR FLUID_DerefProcedure(objScript *, struct scDerefProcedure *);

static const MethodArray clMethods[] = {
   { MT_ScGetProcedureID, (APTR)FLUID_GetProcedureID, "GetProcedureID", NULL, 0 },
   { MT_ScDerefProcedure, (APTR)FLUID_DerefProcedure, "DerefProcedure", NULL, 0 },
   { 0, NULL, NULL, NULL, 0 }
};

//****************************************************************************

static void free_all(objScript *Self)
{
   auto prv = (prvFluid *)Self->Head.ChildPrivate;
   if (!prv) return; // Not a problem - indicates the object did not pass initialisation

   clear_subscriptions(Self);

   if (prv->StateMap) { delete prv->StateMap; prv->StateMap = NULL; }
   if (prv->Structs) { FreeResource(prv->Structs); prv->Structs = NULL; }
   if (prv->Includes) { FreeResource(prv->Includes); prv->Includes = NULL; }
   if (prv->FocusEventHandle) { UnsubscribeEvent(prv->FocusEventHandle); prv->FocusEventHandle = NULL; }

   lua_close(prv->Lua);
   prv->Lua = NULL;
}

//****************************************************************************
// Proxy functions for controlling access to global variables.

static int global_index(lua_State *Lua) // Read global via proxy
{
   lua_rawget(Lua, lua_upvalueindex(1));
   return 1;
}

static int global_newindex(lua_State *Lua) // Write global variable via proxy
{
   if (Lua->ProtectedGlobals) {
      lua_pushvalue(Lua, 2);
      lua_rawget(Lua, lua_upvalueindex(1));
      LONG existing_type = lua_type(Lua, -1);
      lua_pop(Lua, 1);
      if (existing_type == LUA_TFUNCTION) { //(existing_type != LUA_TNIL) {
         luaL_error(Lua, "Unpermitted attempt to overwrite existing global '%s' with a %s type.", luaL_checkstring(Lua, 2), lua_typename(Lua, lua_type(Lua, -1)));
      }
   }

   lua_rawset(Lua, lua_upvalueindex(1));
   return 0;
}

//****************************************************************************
// Only to be used immediately after a failed lua_pcall().  Lua stores a description of the error that occurred on the
// stack, this will be popped and copied to the ErrorString field.

void process_error(objScript *Self, CSTRING Procedure)
{
   auto prv = (prvFluid *)Self->Head.ChildPrivate;

   LONG flags = VLF_WARNING;
   if (prv->CaughtError) {
      Self->Error = prv->CaughtError;
      if (Self->Error <= ERR_Terminate) flags = VLF_EXTAPI; // Non-critical errors are muted to prevent log noise.
   }

   parasol::Log log;
   CSTRING str = lua_tostring(prv->Lua, -1);
   lua_pop(prv->Lua, 1);  // pop returned value
   SetString(Self, FID_ErrorString, str);

   CSTRING file = Self->Path;
   if (file) {
      LONG i;
      for (i=StrLength(file); (i > 0) and (file[i-1] != '/') and (file[i-1] != '\\'); i--);
      log.msg(flags, "%s: %s", file+i, str);
   }
   else log.msg(flags, "%s: Error: %s", Procedure, str);

   // NB: CurrentLine is set by hook_debug(), so if debugging isn't active, you don't know what line we're on.

   if (Self->CurrentLine >= 0) {
      char line[60];
      get_line(Self, Self->CurrentLine, line, sizeof(line));
      log.msg(flags, "Line %d: %s...", Self->CurrentLine+1+Self->LineOffset, line);
   }
}

/*****************************************************************************
** This routine is intended for handling ActionNotify() messages only.  It takes the FunctionField list provided by the
** action and copies them into a table.  Each value is represented by the relevant parameter name for ease of use.
*/

static ERROR stack_args(lua_State *Lua, OBJECTID ObjectID, const FunctionField *args, BYTE *Buffer)
{
   parasol::Log log(__FUNCTION__);
   LONG j;

   if (!args) return ERR_Okay;

   log.traceBranch("Args: %p, Buffer: %p", args, Buffer);

   for (LONG i=0; args[i].Name; i++) {
      char name[StrLength(args[i].Name)+1];
      for (j=0; args[i].Name[j]; j++) name[j] = LCASE(args[i].Name[j]);
      name[j] = 0;

      lua_pushlstring(Lua, name, j);

      // Note: If the object is public and the call was messaged from a foreign process, all strings/pointers are
      // invalid because the message handlers cannot do deep pointer resolution of the structure we receive from
      // ActionNotify.

      if (args[i].Type & FD_STR) {
         if (ObjectID > 0) lua_pushstring(Lua, ((STRING *)Buffer)[0]);
         else lua_pushnil(Lua);
         Buffer += sizeof(STRING);
      }
      else if (args[i].Type & FD_PTR) {
         if (ObjectID > 0) lua_pushlightuserdata(Lua, ((APTR *)Buffer)[0]);
         else lua_pushnil(Lua);
         Buffer += sizeof(APTR);
      }
      else if (args[i].Type & FD_LONG) {
         lua_pushinteger(Lua, ((LONG *)Buffer)[0]);
         Buffer += sizeof(LONG);
      }
      else if (args[i].Type & FD_DOUBLE) {
         lua_pushnumber(Lua, ((DOUBLE *)Buffer)[0]);
         Buffer += sizeof(DOUBLE);
      }
      else if (args[i].Type & FD_LARGE) {
         lua_pushnumber(Lua, ((LARGE *)Buffer)[0]);
         Buffer += sizeof(LARGE);
      }
      else {
         log.warning("Unsupported arg %s, flags $%.8x, aborting now.", args[i].Name, args[i].Type);
         return ERR_Failed;
      }
      lua_settable(Lua, -3);
   }

   return ERR_Okay;
}

//****************************************************************************
// Action notifications arrive when the user has used object.subscribe() in the Fluid script.
//
// function(ObjectID, Args, Reference)


static ERROR FLUID_ActionNotify(objScript *Self, struct acActionNotify *Args)
{
   if (!Args) return ERR_NullArgs;
   if (Args->Error != ERR_Okay) return ERR_Okay;

   auto prv = (prvFluid *)Self->Head.ChildPrivate;
   if (!prv) return ERR_Okay;

   for (auto scan=prv->ActionList; scan; scan=scan->Next) {
      if ((Args->ObjectID IS scan->ObjectID) and (Args->ActionID IS scan->ActionID)) {
         LONG depth = GetResource(RES_LOG_DEPTH); // Required because thrown errors cause the debugger to lose its branch

         {
            parasol::Log log;

            log.msg(VLF_BRANCH|VLF_EXTAPI, "Action notification for object #%d, action %d.  Top: %d", Args->ObjectID, Args->ActionID, lua_gettop(prv->Lua));

            lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, scan->Function); // +1 stack: Get the function reference
            push_object_id(prv->Lua, Args->ObjectID);  // +1: Pass the object ID
            lua_newtable(prv->Lua);  // +1: Table to store the parameters
            if (!stack_args(prv->Lua, Args->ObjectID, scan->Args, (STRING)Args->Args)) {
               LONG total_args;
               if (scan->Reference) { // +1: Custom reference (optional)
                  lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, scan->Reference);
                  total_args = 3; // ObjectID, ArgTable, Reference
               }
               else total_args = 2; // ObjectID, ArgTable

               if (lua_pcall(prv->Lua, total_args, 0, 0)) { // Make the call, function & args are removed from stack.
                  process_error(Self, "Action Subscription");
               }

               log.msg(VLF_BRANCH|VLF_EXTAPI, "Collecting garbage.");
               lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
            }
         }

         SetResource(RES_LOG_DEPTH, depth);
         return ERR_Okay;
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR FLUID_Activate(objScript *Self, APTR Void)
{
   parasol::Log log;

   if (CurrentTaskID() != Self->Head.TaskID) return log.warning(ERR_IllegalActionAttempt);
   if ((!Self->String) or (!Self->String[0])) return log.warning(ERR_FieldNotSet);

   log.msg(VLF_EXTAPI, "Target: %d, Procedure: %s / ID #" PF64(), Self->TargetID, Self->Procedure ? Self->Procedure : (STRING)".", Self->ProcedureID);

   ERROR error = ERR_Failed;

   auto prv = (prvFluid *)Self->Head.ChildPrivate;
   if (!prv) return log.warning(ERR_ObjectCorrupt);

   if (prv->Recurse) { // When performing a recursive call, we can assume that the code has already been loaded.
      error = run_script(Self);
      if (error) Self->Error = error;

      {
         parasol::Log log;
         log.traceBranch("Collecting garbage.");
         lua_gc(prv->Lua, LUA_GCCOLLECT, 0);
      }

      return ERR_Okay;
   }

   prv->Recurse++;

   Self->CurrentLine = -1;
   Self->Error       = ERR_Okay;

   // Set the script owner to the current process, prior to script execution.  Once complete, we will change back to
   // the original owner.

   Self->ScriptOwnerID = Self->Head.OwnerID;
   OBJECTID owner_id = GetOwner(Self);
   SetOwner(Self, CurrentTask());

   BYTE reload = FALSE;
   if (!Self->ActivationCount) reload = TRUE;

   LONG i, j;
   if ((Self->ActivationCount) and (!Self->Procedure) and (!Self->ProcedureID)) {
      // If no procedure has been specified, kill the old Lua instance to restart from scratch

      FLUID_Free(Self, NULL);

      if (!(prv->Lua = luaL_newstate())) {
         log.warning("Failed to open a Lua instance.");
         goto failure;
      }

      reload = TRUE;
   }

   if (reload) {
      log.trace("The Lua script will be initialised from scratch.");

      prv->Lua->Script = Self;
      prv->Lua->ProtectedGlobals = FALSE;

      // Change the __newindex and __index methods of the global table so that all access passes
      // through a proxy table that we control.

      lua_newtable(prv->Lua); // Storage table
      {
         lua_newtable(prv->Lua); // table = { __newindex = A, __index = B }
         {
            lua_pushstring(prv->Lua, "__newindex");
            lua_pushvalue(prv->Lua, 1); // Storage table
            lua_pushcclosure(prv->Lua, global_newindex, 1);
            lua_settable(prv->Lua, -3);

            lua_pushstring(prv->Lua, "__index");
            lua_pushvalue(prv->Lua, 1);
            lua_pushcclosure(prv->Lua, global_index, 1);
            lua_settable(prv->Lua, -3);
         }
         lua_setmetatable(prv->Lua, LUA_GLOBALSINDEX);
      }
      lua_pop(prv->Lua, 1); // Pop the storage table

      lua_gc(prv->Lua, LUA_GCSTOP, 0);  // Stop collector during initialization
         luaL_openlibs(prv->Lua);  // Open Lua libraries
      lua_gc(prv->Lua, LUA_GCRESTART, 0);

      // Register private variables in the registry, which is tamper proof from the user's Lua code

      if (register_interfaces(Self) != ERR_Okay) goto failure;

      // Line hook, executes on the execution of a new line

      if (Self->Flags & SCF_DEBUG) {
         // LUA_MASKLINE:  Interpreter is executing a line.
         // LUA_MASKCALL:  Interpreter is calling a function.
         // LUA_MASKRET:   Interpreter returns from a function.
         // LUA_MASKCOUNT: The hook will be called every X number of instructions executed.

         lua_sethook(prv->Lua, hook_debug, LUA_MASKCALL|LUA_MASKRET|LUA_MASKLINE, 0);
      }

      // Pre-load the Core module: mSys = mod.load('core')

      OBJECTPTR module;
      if (!CreateObject(ID_MODULE, 0, &module, FID_Name|TSTR, "core", TAGEND)) {
         auto mod = (struct module *)lua_newuserdata(prv->Lua, sizeof(struct module));
         ClearMemory(mod, sizeof(struct module));
         luaL_getmetatable(prv->Lua, "Fluid.mod");
         lua_setmetatable(prv->Lua, -2);
         mod->Module = module;
         GetPointer(module, FID_FunctionList, &mod->Functions);
         lua_setglobal(prv->Lua, "mSys");
      }
      else {
         log.warning("Failed to create module object.");
         goto failure;
      }

      prv->Lua->ProtectedGlobals = TRUE;

      LONG result;
      if (!StrCompare(LUA_COMPILED, Self->String, 0, 0)) { // The source is compiled
         log.trace("Loading pre-compiled Lua script.");
         LONG headerlen = StrLength(Self->String) + 1;
         result = luaL_loadbuffer(prv->Lua, Self->String + headerlen, prv->LoadedSize - headerlen, "DefaultChunk");
      }
      else {
         log.trace("Compiling Lua script.");
         result = luaL_loadstring(prv->Lua, Self->String);
      }

      if (result) { // Error reported from parser
         CSTRING errorstr;
         if ((errorstr = lua_tostring(prv->Lua,-1))) {
            // Format: [string "..."]:Line:Error
            if ((i = StrSearch("\"]:", errorstr, STR_MATCH_CASE)) != -1) {
               char buffer[240];
               i += 3;
               LONG line = StrToInt(errorstr + i);
               while ((errorstr[i]) and (errorstr[i] != ':')) i++;
               if (errorstr[i] IS ':') i++;
               i = StrFormat(buffer, sizeof(buffer), "Line %d: %s\n", line+Self->LineOffset, errorstr + i);
               CSTRING str = Self->String;

               for (j=1; j <= line+1; j++) {
                  if (j >= line-1) {
                     i += StrFormat(buffer+i, sizeof(buffer)-i, "%d: ", j+Self->LineOffset);
                     LONG k;
                     for (k=0; (str[k]) and (str[k] != '\n') and (str[k] != '\r') and (k < 120); k++) {
                        if ((size_t)i >= sizeof(buffer)-1) break;
                        buffer[i++] = str[k];
                     }
                     if (k IS 120) {
                        for (k=0; (k < 3) and ((size_t)i < sizeof(buffer)-1); k++) buffer[i++] = '.';
                     }
                     if ((size_t)i < sizeof(buffer)-1) buffer[i++] = '\n';
                     buffer[i] = 0;
                  }
                  if (!(str = StrNextLine(str))) break;
               }
               SetString(Self, FID_ErrorString, buffer);

               log.warning("Parser Failed: %s", Self->ErrorString);
            }
            else log.warning("Parser Failed: %s", errorstr);
         }

         lua_pop(prv->Lua, 1);  // Pop error string
         goto failure;
      }
      else log.trace("Script successfully compiled.");

      if (prv->SaveCompiled) { // Compile the script and save the result to the cache file
         log.msg("Compiling the source into the cache file.");

         prv->SaveCompiled = FALSE;

         objFile *cachefile;
         if (!CreateObject(ID_FILE, NF_INTEGRAL, &cachefile,
               FID_Path|TSTR,         Self->CacheFile,
               FID_Flags|TLONG,       FL_NEW|FL_WRITE,
               FID_Permissions|TLONG, prv->CachePermissions,
               TAGEND)) {

            save_binary(Self, cachefile->Head.UID);

            SetPointer(cachefile, FID_Date, &prv->CacheDate);
            acFree(cachefile);
         }
      }
   }
   else log.trace("Using the Lua script cache.");

   Self->ActivationCount++;

   if ((Self->Procedure) or (Self->ProcedureID)) {
      // The Lua script needs to have been executed at least once in order for the procedures to be initialised and recognised.

      if ((Self->ActivationCount IS 1) or (reload)) {
         parasol::Log log;
         log.traceBranch("Collecting functions prior to procedure call...");

         if (lua_pcall(prv->Lua, 0, 0, 0)) {
            Self->Error = ERR_Failed;
            process_error(Self, "Activation");
         }
      }
   }

   if (!Self->Error) {
      run_script(Self); // Will set Self->Error if there's an issue
   }

   error = ERR_Okay; // The error reflects on the initial processing of the script only - the developer must check the Error field for information on script execution

failure:

   if (prv->Lua) {
      parasol::Log log;
      log.traceBranch("Collecting garbage.");
      lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
   }

   // Change back to the original owner if it still exists.  If it doesn't, self-terminate.

   if (owner_id) {
      OBJECTPTR owner;
      if (!AccessObject(owner_id, 5000, &owner)) {
         SetOwner(Self, owner);
         ReleaseObject(owner);
      }
      else {
         log.msg("Owner #%d no longer exists - self-terminating.", owner_id);
         acFree(Self);
      }
   }

   Self->ScriptOwnerID = 0;
   prv->Recurse--;
   return error;
}

//****************************************************************************

static ERROR FLUID_DataFeed(objScript *Self, struct acDataFeed *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;

   if (Args->DataType IS DATA_TEXT) {
      SetString(Self, FID_String, (CSTRING)Args->Buffer);
   }
   else if (Args->DataType IS DATA_XML) {
      SetString(Self, FID_String, (CSTRING)Args->Buffer);
   }
   else if (Args->DataType IS DATA_RECEIPT) {
      auto prv = (prvFluid *)Self->Head.ChildPrivate;
      struct datarequest *prev;

      log.branch("Incoming data receipt from #%d", Args->ObjectID);

restart:
      prev = NULL;
      for (auto list=prv->Requests; list; list=list->Next) {
         if (list->SourceID IS Args->ObjectID) {
            // Execute the callback associated with this input subscription: function({Items...})

            LONG step = GetResource(RES_LOG_DEPTH); // Required as thrown errors cause the debugger to lose its step position

               lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, list->Callback); // +1 Reference to callback
               lua_newtable(prv->Lua); // +1 Item table

               objXML *xml;
               if (!CreateObject(ID_XML, NF_INTEGRAL, &xml, FID_Statement|TSTR, Args->Buffer, TAGEND)) {
                  // <file path="blah.exe"/> becomes { item='file', path='blah.exe' }

                  XMLTag *tag = xml->Tags[0];
                  LONG i = 1;
                  if (!StrMatch("receipt", tag->Attrib->Name)) {
                     for (auto scan=tag->Child; scan; scan=scan->Next) {
                        lua_pushinteger(prv->Lua, i++);
                        lua_newtable(prv->Lua);

                        lua_pushstring(prv->Lua, "item");
                        lua_pushstring(prv->Lua, scan->Attrib->Name);
                        lua_settable(prv->Lua, -3);

                        for (LONG a=1; a < scan->TotalAttrib; a++) {
                           lua_pushstring(prv->Lua, scan->Attrib[a].Name);
                           lua_pushstring(prv->Lua, scan->Attrib[a].Value);
                           lua_settable(prv->Lua, -3);
                        }

                        lua_settable(prv->Lua, -3);
                     }
                  }

                  acFree(xml);

                  if (lua_pcall(prv->Lua, 1, 0, 0)) { // function(Items)
                     process_error(Self, "Data Receipt Callback");
                  }
               }

            SetResource(RES_LOG_DEPTH, step);

            if (!prev) prv->Requests = list->Next;
            else prev->Next = list->Next;
            FreeResource(list);
            goto restart;
         }

         prev = list;
      }

      {
         parasol::Log log;
         log.traceBranch("Collecting garbage.");
         lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR FLUID_DerefProcedure(objScript *Self, struct scDerefProcedure *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;

   if ((Args->Procedure) and (Args->Procedure->Type IS CALL_SCRIPT)) {
      if (Args->Procedure->Script.Script IS &Self->Head) { // Verification of ownership
         auto prv = (prvFluid *)Self->Head.ChildPrivate;
         if (!prv) return log.warning(ERR_ObjectCorrupt);

         log.trace("Dereferencing procedure #" PF64(), Args->Procedure->Script.ProcedureID);

         if (Args->Procedure->Script.ProcedureID) {
            luaL_unref(prv->Lua, LUA_REGISTRYINDEX, Args->Procedure->Script.ProcedureID);
            Args->Procedure->Script.ProcedureID = 0;
         }
         return ERR_Okay;
      }
      else return log.warning(ERR_Args);
   }
   else return log.warning(ERR_Args);
}

//****************************************************************************

static ERROR FLUID_Free(objScript *Self, APTR Void)
{
   free_all(Self);
   return ERR_Okay;
}

//****************************************************************************

static ERROR FLUID_GetProcedureID(objScript *Self, struct scGetProcedureID *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Procedure) or (!Args->Procedure[0])) return log.warning(ERR_NullArgs);

   auto prv = (prvFluid *)Self->Head.ChildPrivate;
   if (!prv) return log.warning(ERR_ObjectCorrupt);

   if ((!prv->Lua) or (!Self->ActivationCount)) {
      log.warning("Cannot resolve function '%s'.  Script requires activation.", Args->Procedure);
      return ERR_NotFound;
   }

   lua_getglobal(prv->Lua, Args->Procedure);
   LONG id = luaL_ref(prv->Lua, LUA_REGISTRYINDEX);
   if ((id != LUA_REFNIL) and (id != LUA_NOREF)) {
      Args->ProcedureID = id;
      return ERR_Okay;
   }
   else {
      log.warning("Failed to resolve function name '%s' to an ID.", Args->Procedure);
      return ERR_NotFound;
   }
}

//****************************************************************************

static ERROR FLUID_Init(objScript *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Path) {
      if (StrCompare("*.fluid|*.fb|*.lua", Self->Path, 0, STR_WILDCARD) != ERR_Okay) {
         log.trace("No support for path '%s'", Self->Path);
         return ERR_NoSupport;
      }
   }

   if ((Self->Head.Flags & NF_RECLASSED) and (!Self->String)) {
      log.trace("No support for reclassed Script with no String field value.");
      return ERR_NoSupport;
   }

   STRING str;
   ERROR error;
   BYTE compile = FALSE;
   LONG loaded_size = 0;
   parasol::ScopedObject<objFile> src_file;
   if ((!Self->String) and (Self->Path)) {
      error = CreateObject(ID_FILE, NF_INTEGRAL, &src_file.obj, FID_Path|TSTR, Self->Path, TAGEND);

      LARGE src_ts, src_size;
      if (!error) {
         error = GetFields(src_file.obj, FID_TimeStamp|TLARGE, &src_ts, FID_Size|TLARGE, &src_size, TAGEND);
      }
      else error = ERR_File;

      if (Self->CacheFile) {
         // Compare the cache file date to the original source.  If they match, or if there was a problem
         // analysing the original location (i.e. the original location does not exist) then the cache file is loaded
         // instead of the original source code.

         objFile *cache_file;
         if (!CreateObject(ID_FILE, NF_INTEGRAL, &cache_file, FID_Path|TSTR, Self->CacheFile, TAGEND)) {
            LARGE cache_ts, cache_size;
            GetFields(cache_file, FID_TimeStamp|TLARGE, &cache_ts, FID_Size|TLARGE, &cache_size, TAGEND);
            acFree(cache_file);

            if ((cache_ts IS src_ts) or (error)) {
               log.msg("Using cache '%s'", Self->CacheFile);
               if (!AllocMemory(cache_size, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, &Self->String, NULL)) {
                  LONG len;
                  error = ReadFileToBuffer(Self->CacheFile, Self->String, cache_size, &len);
                  loaded_size = cache_size;
               }
               else error = ERR_AllocMemory;
            }
         }
      }

      if ((!error) and (!loaded_size)) {
         if (!AllocMemory(src_size+1, MEM_STRING|MEM_NO_CLEAR, &Self->String, NULL)) {
            LONG len;
            if (!ReadFileToBuffer(Self->Path, Self->String, src_size, &len)) {
               Self->String[len] = 0;

               // Unicode BOM handler - in case the file starts with a BOM header.
               CSTRING bomptr = check_bom(Self->String);
               if (bomptr != Self->String) CopyMemory(bomptr, Self->String, (len + 1) - (bomptr - Self->String));

               loaded_size = len;

               if (Self->CacheFile) compile = TRUE; // Saving a compilation of the source is desired
            }
            else {
               log.trace("Failed to read " PF64() " bytes from '%s'", src_size, Self->Path);
               FreeResource(Self->String);
               Self->String = NULL;
               error = ERR_ReadFileToBuffer;
            }
         }
         else error = ERR_AllocMemory;
      }
   }
   else error = ERR_Okay;

   // Allocate private structure

   prvFluid *prv;
   if (!error) {
      if (!AllocMemory(sizeof(prvFluid), Self->Head.MemFlags, &Self->Head.ChildPrivate, NULL)) {
         prv = (prvFluid *)Self->Head.ChildPrivate;
         if ((prv->SaveCompiled = compile)) {
            DateTime *dt;
            if (!GetPointer(src_file.obj, FID_Date, &dt)) prv->CacheDate = *dt;
            GetLong(src_file.obj, FID_Permissions, &prv->CachePermissions);
            prv->LoadedSize = loaded_size;
         }
      }
      else error = ERR_AllocMemory;
   }

   if (error) return log.warning(error);

   //if (!(prv->ObjectList = VarNew(0, 0))) return log.warning(ERR_AllocMemory);

   log.trace("Opening a Lua instance.");

   if (!(prv->Lua = luaL_newstate())) {
      log.warning("Failed to open a Lua instance.");
      FreeResource(Self->Head.ChildPrivate);
      Self->Head.ChildPrivate = NULL;
      return ERR_Failed;
   }

   if (!(str = Self->String)) {
      log.trace("No statement specified at this stage.");
      return ERR_Okay; // Assume that the script's text will be incoming later
   }

   // Search for a $FLUID comment - this can contain extra details and options for the script.  Valid identifiers are
   //
   //    -- $FLUID
   //    \* $FLUID
   //    // $FLUID

   if (!StrCompare("?? $FLUID", str, 0, STR_WILDCARD)) {

   }

   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
SaveToObject: Compiles the current script statement and saves it as byte code.

Use the SaveToObject action to compile the statement in the Script's String field and save the resulting byte code to a
target object.  The byte code can be loaded into any script object for execution or referenced in the Fluid code for
usage.

*****************************************************************************/

static ERROR FLUID_SaveToObject(objScript *Self, struct acSaveToObject *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->DestID)) return log.warning(ERR_NullArgs);

   if (!Self->String) return log.warning(ERR_FieldNotSet);

   log.branch("Compiling the statement...");

   auto prv = (prvFluid *)Self->Head.ChildPrivate;
   if (!prv) return log.warning(ERR_ObjectCorrupt);

   if (!luaL_loadstring(prv->Lua, Self->String)) {
      ERROR error = save_binary(Self, Args->DestID);
      return error;
   }
   else {
      CSTRING str = lua_tostring(prv->Lua,-1);
      lua_pop(prv->Lua, 1);
      log.warning("Compile Failure: %s", str);
      return ERR_InvalidData;
   }
}

/*****************************************************************************

-FIELD-
Procedures: Returns a string array of all named procedures defined by a script.

A string array of all procedures loaded into a script is returned by this function. The script will need to have been
activated before reading this field, or an empty list will be returned.

The procedure list is built at the time of the call.  The array is allocated as a memory block and will need to be
removed by the caller with FreeResource().
-END-

*****************************************************************************/

static ERROR GET_Procedures(objScript *Self, STRING **Value, LONG *Elements)
{
   auto prv = (prvFluid *)Self->Head.ChildPrivate;
   if (prv) {
      LONG memsize = 1024 * 64;
      UBYTE *list;
      if (!AllocMemory(memsize, MEM_DATA|MEM_NO_CLEAR, &list, NULL)) {
         LONG total = 0;
         LONG size  = 0;
         lua_pushnil(prv->Lua);
         while (lua_next(prv->Lua, LUA_GLOBALSINDEX)) {
            if (lua_type(prv->Lua, -1) IS LUA_TFUNCTION) {
               CSTRING name = lua_tostring(prv->Lua, -2);
               size += StrCopy(name, (STRING)list+size, memsize - size) + 1;
               total++;
            }
            lua_pop(prv->Lua, 1);
         }

         *Value = StrBuildArray((STRING)list, size, total, SBF_SORT);
         *Elements = total;

         FreeResource(list);
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return ERR_Failed;
}

//****************************************************************************

static ERROR save_binary(objScript *Self, OBJECTID FileID)
{
   #warning No support for save_binary() yet.

   return ERR_NoSupport;
/*
   prvFluid *prv;
   OBJECTPTR dest;
   const Proto *f;
   LONG i;

   LogF("~save_binary()","Save Symbols: %d", Self->Flags & SCF_DEBUG);

   if (!(prv = Self->Head.ChildPrivate)) return LogReturnError(0, ERR_ObjectCorrupt);

   f = clvalue(prv->Lua->top + (-1))->l.p;

   // Write the fluid header first.  This must identify the content as compiled, plus include any relevant options,
   // such as the persistent identifier.

   if (!AccessObject(FileID, 3000, &dest)) {
      LONG result;
      UBYTE header[256];

      i = StrCopy(LUA_COMPILED, header, sizeof(header));
      header[i++] = 0;

      if (acWrite(dest, header, i, &result)) {
         ReleaseObject(dest);
         return LogReturnError(0, ERR_Write);
      }
      ReleaseObject(dest);
   }

   if ((code_writer_id > 0) and ((dest = GetObjectPtr(FileID)))) {
      luaU_dump(prv->Lua, f, &code_writer, dest, (Self->Flags & SCF_DEBUG) ? 0 : 1);
   }
   else luaU_dump(prv->Lua, f, &code_writer_id, (void *)(MAXINT)FileID, (Self->Flags & SCF_DEBUG) ? 0 : 1);

   LogReturn();
   return ERR_Okay;
*/
}

//****************************************************************************

static ERROR run_script(objScript *Self)
{
   parasol::Log log(__FUNCTION__);

   auto prv = (prvFluid *)Self->Head.ChildPrivate;

   log.traceBranch("Procedure: %s, Top: %d", Self->Procedure, lua_gettop(prv->Lua));

   prv->CaughtError = ERR_Okay;
   struct object * release_list[8];
   LONG r = 0;
   LONG top;
   LONG pcall_failed = FALSE;
   if ((Self->Procedure) or (Self->ProcedureID)) {
      if (Self->Procedure) lua_getglobal(prv->Lua, Self->Procedure);
      else lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, Self->ProcedureID);

      if (lua_isfunction(prv->Lua, -1)) {
         if (Self->Flags & SCF_DEBUG) log.branch("Executing procedure: %s, Args: %d", Self->Procedure, Self->TotalArgs);

         top = lua_gettop(prv->Lua);

         LONG count = 0;
         const ScriptArg *args;
         if ((args = Self->ProcArgs)) {
            for (LONG i=0; i < Self->TotalArgs; i++, args++) {
               LONG type = args->Type;

               if (type & FD_ARRAY) {
                  log.trace("Setting arg '%s', Array: %p", args->Name, args->Address);

                  APTR values = args->Address;
                  LONG total_elements = -1;
                  CSTRING arg_name = args->Name;
                  if (args[1].Type & FD_ARRAYSIZE) {
                     if (args[1].Type & FD_LONG) total_elements = args[1].Long;
                     else if (args[1].Type & FD_LARGE) total_elements = args[1].Large;
                     else values = NULL;
                     i++; args++; // Because we took the array-size parameter into account
                  }
                  else log.trace("The size of the array is not defined.");

                  if (values) {
                     make_any_table(prv->Lua, type, arg_name, total_elements, values);

                     if (type & FD_ALLOC) FreeResource(values);
                  }
                  else lua_pushnil(prv->Lua);
               }
               else if (type & FD_STR) {
                  log.trace("Setting arg '%s', Value: %.20s", args->Name, (CSTRING)args->Address);
                  lua_pushstring(prv->Lua, (CSTRING)args->Address);
               }
               else if (type & FD_STRUCT) {
                  // Pointer to a struct, which can be referenced with a name of "StructName" or "StructName:ArgName"
                  if (args->Address) {
                     if (named_struct_to_table(prv->Lua, args->Name, args->Address) != ERR_Okay) lua_pushnil(prv->Lua);
                     if (type & FD_ALLOC) FreeResource(args->Address);
                  }
                  else lua_pushnil(prv->Lua);
               }
               else if (type & (FD_PTR|FD_BUFFER)) {
                  // Try and make the pointer safer/more usable by translating it into a buffer, object ID or whatever.
                  // (In a secure environment, pointers may be passed around but may be useless if their use is
                  // disallowed within Lua).

                  log.trace("Setting arg '%s', Value: %p", args->Name, args->Address);
                  if ((type & FD_BUFFER) and (i+1 < Self->TotalArgs) and (args[1].Type & FD_BUFSIZE)) {
                     // Buffers are considered to be directly writable regions of memory, so the array interface is
                     // used to represent them.
                     if (args[1].Type & FD_LONG) make_array(prv->Lua, FD_BYTE|FD_WRITE, NULL, (APTR *)args->Address, args[1].Long, FALSE);
                     else if (args[1].Type & FD_LARGE) make_array(prv->Lua, FD_BYTE|FD_WRITE, NULL, (APTR *)args->Address, args[1].Large, FALSE);
                     else lua_pushnil(prv->Lua);
                     i++; args++; // Because we took the buffer-size parameter into account
                  }
                  else if (type & FD_OBJECT) {
                     // Pushing direct object pointers is considered safe because they are treated as detached, then
                     // a lock is gained for the duration of the call that is then released on return.  This is a
                     // solid optimisation that also protects the object from unwarranted termination during the call.

                     if (args->Address) {
                        struct object *obj = push_object(prv->Lua, (OBJECTPTR)args->Address);
                        if ((r < ARRAYSIZE(release_list)) and (access_object(obj))) {
                           release_list[r++] = obj;
                        }
                     }
                     else lua_pushnil(prv->Lua);
                  }
                  else lua_pushlightuserdata(prv->Lua, args->Address);
               }
               else if (type & FD_LONG)   {
                  log.trace("Setting arg '%s', Value: %d", args->Name, args->Long);
                  if (type & FD_OBJECT) {
                     if (args->Long) push_object_id(prv->Lua, args->Long);
                     else lua_pushnil(prv->Lua);
                  }
                  else lua_pushinteger(prv->Lua, args->Long);
               }
               else if (type & FD_LARGE)  { log.trace("Setting arg '%s', Value: " PF64(), args->Name, args->Large); lua_pushnumber(prv->Lua, args->Large); }
               else if (type & FD_DOUBLE) { log.trace("Setting arg '%s', Value: %.2f", args->Name, args->Double); lua_pushnumber(prv->Lua, args->Double); }
               else { lua_pushnil(prv->Lua); log.warning("Arg '%s' uses unrecognised type $%.8x", args->Name, type); }
               count++;
            }
         }

         LONG step = GetResource(RES_LOG_DEPTH);

         if (lua_pcall(prv->Lua, count, LUA_MULTRET, 0)) {
            pcall_failed = TRUE;
         }

         SetResource(RES_LOG_DEPTH, step);

         while (r > 0) release_object(release_list[--r]);
      }
      else {
         char buffer[200];
         StrFormat(buffer, sizeof(buffer), "Procedure '%s' / #" PF64() " does not exist in the script.", Self->Procedure, Self->ProcedureID);
         SetString(Self, FID_ErrorString, buffer);
         log.warning("%s", buffer);

         #ifdef DEBUG
            STRING *list;
            LONG total_procedures;
            if (!GET_Procedures(Self, &list, &total_procedures)) {
               for (LONG i=0; i < total_procedures; i++) log.trace("%s", list[i]);
               FreeResource(list);
            }
         #endif

         Self->Error = ERR_NotFound;
         return ERR_NotFound;
      }
   }
   else {
      LONG depth = GetResource(RES_LOG_DEPTH);

         top = lua_gettop(prv->Lua);
         if (lua_pcall(prv->Lua, 0, LUA_MULTRET, 0)) {
            pcall_failed = TRUE;
         }

      SetResource(RES_LOG_DEPTH, depth);
   }

   if (!pcall_failed) { // If the procedure returned results, copy them to the Results field of the Script.
      LONG results = lua_gettop(prv->Lua) - top + 1;

      if (results > 0) {
         CSTRING array[results+1];
         LONG i;

         // NB: The Results field will take a clone of the Lua strings, so this sub-routine is safe to pass
         // on Lua's temporary string results.

         for (i=0; i < results; i++) {
            array[i] = lua_tostring(prv->Lua, -results+i);
            log.trace("Result: %d/%d: %s", i, results, array[i]);
         }
         array[i] = NULL;
         SetArray(Self, FID_Results, array, i);
         lua_pop(prv->Lua, results);  // pop returned values
      }

      return ERR_Okay;
   }
   else {
      process_error(Self, Self->Procedure ? Self->Procedure : "run_script");
      return Self->Error;
   }
}
//****************************************************************************

static ERROR register_interfaces(objScript *Self)
{
   parasol::Log log;

   log.traceBranch("Registering Parasol and Fluid interfaces with Lua.");

   auto prv = (prvFluid *)Self->Head.ChildPrivate;

   register_array_class(prv->Lua);
   register_object_class(prv->Lua);
   register_module_class(prv->Lua);
   register_struct_class(prv->Lua);
   register_thread_class(prv->Lua);
   register_input_class(prv->Lua);
   register_number_class(prv->Lua);
   register_processing_class(prv->Lua);

   lua_register(prv->Lua, "arg", fcmd_arg);
   lua_register(prv->Lua, "catch", fcmd_catch);
   lua_register(prv->Lua, "check", fcmd_check);
   lua_register(prv->Lua, "raise", fcmd_raise);
   lua_register(prv->Lua, "loadFile", fcmd_loadfile);
   lua_register(prv->Lua, "exec", fcmd_exec);
   lua_register(prv->Lua, "getExecutionState", fcmd_get_execution_state);
   lua_register(prv->Lua, "print", fcmd_print);
   lua_register(prv->Lua, "include", fcmd_include);
   lua_register(prv->Lua, "require", fcmd_require);
   lua_register(prv->Lua, "msg", fcmd_msg);
   lua_register(prv->Lua, "nz", fcmd_nz);
   lua_register(prv->Lua, "subscribeEvent", fcmd_subscribe_event);
   lua_register(prv->Lua, "unsubscribeEvent", fcmd_unsubscribe_event);
   lua_register(prv->Lua, "MAKESTRUCT", MAKESTRUCT);

   load_include(Self, "core");

   return ERR_Okay;
}

//****************************************************************************

ERROR create_fluid(void)
{
   return(CreateObject(ID_METACLASS, 0, &clFluid,
      FID_BaseClassID|TLONG,    ID_SCRIPT,
      FID_SubClassID|TLONG,     ID_FLUID,
      FID_ClassVersion|TFLOAT,  VER_FLUID,
      FID_Name|TSTR,            "Fluid",
      FID_Category|TLONG,       CCF_DATA,
      FID_FileExtension|TSTR,   "*.fluid|*.fb|*.lua",
      FID_FileDescription|TSTR, "Fluid",
      FID_Actions|TPTR,         clActions,
      FID_Methods|TARRAY,       clMethods,
      FID_Fields|TARRAY,        clFields,
      FID_Path|TSTR,            MOD_PATH,
      TAGEND));
}
