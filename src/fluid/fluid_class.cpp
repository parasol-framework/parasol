
#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/display.h>
#include <parasol/modules/fluid.h>
#include <parasol/strings.hpp>
#include <sstream>
#include <string_view>

#include "lua.hpp"

extern "C" {
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

static ERR run_script(objScript *);
static ERR stack_args(lua_State *, OBJECTID, const FunctionField *, int8_t *);
static ERR save_binary(objScript *, OBJECTPTR);

inline CSTRING check_bom(CSTRING Value)
{
   if (((char)Value[0] IS 0xef) and ((char)Value[1] IS 0xbb) and ((char)Value[2] IS 0xbf)) Value += 3; // UTF-8 BOM
   else if (((char)Value[0] IS 0xfe) and ((char)Value[1] IS 0xff)) Value += 2; // UTF-16 BOM big endian
   else if (((char)Value[0] IS 0xff) and ((char)Value[1] IS 0xfe)) Value += 2; // UTF-16 BOM little endian
   return Value;
}

// Dump the variables of any global table

static ERR register_interfaces(objScript *) __attribute__ ((unused));
static void dump_global_table(objScript *, STRING Global) __attribute__ ((unused));

static void dump_global_table(objScript *Self, STRING Global)
{
   pf::Log log("print_env");
   lua_State *lua = ((prvFluid *)Self->ChildPrivate)->Lua;
   lua_getglobal(lua, Global);
   if (lua_istable(lua, -1) ) {
      lua_pushnil(lua);
      while (lua_next(lua, -2) != 0) {
         int type = lua_type(lua, -2);
         log.msg("%s = %s", lua_tostring(lua, -2), lua_typename(lua, type));
         lua_pop(lua, 1);
      }
   }
}

//********************************************************************************************************************

static ERR GET_Procedures(objScript *, pf::vector<std::string> **, int *);

static const FieldArray clFields[] = {
   { "Procedures", FDF_VIRTUAL|FDF_CPP|FDF_ARRAY|FDF_STRING|FDF_R, GET_Procedures },
   END_FIELD
};

//********************************************************************************************************************

static ERR FLUID_Activate(objScript *);
static ERR FLUID_DataFeed(objScript *, struct acDataFeed *);
static ERR FLUID_Free(objScript *);
static ERR FLUID_Init(objScript *);
static ERR FLUID_NewChild(objScript *, struct acNewChild &);
static ERR FLUID_SaveToObject(objScript *, struct acSaveToObject *);

static const ActionArray clActions[] = {
   { AC::Activate,     FLUID_Activate },
   { AC::DataFeed,     FLUID_DataFeed },
   { AC::Free,         FLUID_Free },
   { AC::Init,         FLUID_Init },
   { AC::NewChild,     FLUID_NewChild },
   { AC::SaveToObject, FLUID_SaveToObject },
   { AC::NIL, nullptr }
};

//********************************************************************************************************************

static ERR FLUID_GetProcedureID(objScript *, struct sc::GetProcedureID *);
static ERR FLUID_DerefProcedure(objScript *, struct sc::DerefProcedure *);
static ERR FLUID_DebugLog(objScript *, struct sc::DebugLog *);

static const MethodEntry clMethods[] = {
   { sc::GetProcedureID::id, (APTR)FLUID_GetProcedureID, "GetProcedureID", nullptr, 0 },
   { sc::DerefProcedure::id, (APTR)FLUID_DerefProcedure, "DerefProcedure", nullptr, 0 },
   { sc::DebugLog::id,       (APTR)FLUID_DebugLog,       "DebugLog", nullptr, 0 },
   { AC::NIL, nullptr, nullptr, nullptr, 0 }
};

//********************************************************************************************************************
// NOTE: Be aware that this function can be called by Activate() to perform a complete state reset.

static void free_all(objScript *Self)
{
   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv) return; // Not a problem - indicates the object did not pass initialisation

   if (prv->FocusEventHandle) { UnsubscribeEvent(prv->FocusEventHandle); prv->FocusEventHandle = nullptr; }

   auto lua = prv->Lua;
   prv->~prvFluid();

   lua_close(lua);
   prv->Lua = nullptr;
}

//********************************************************************************************************************
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
      int existing_type = lua_type(Lua, -1);
      lua_pop(Lua, 1);
      if (existing_type == LUA_TFUNCTION) { //(existing_type != LUA_TNIL) {
         luaL_error(Lua, "Unpermitted attempt to overwrite existing global '%s' with a %s type.", luaL_checkstring(Lua, 2), lua_typename(Lua, lua_type(Lua, -1)));
      }
   }

   lua_rawset(Lua, lua_upvalueindex(1));
   return 0;
}

//********************************************************************************************************************
// Only to be used immediately after a failed lua_pcall().  Lua stores a description of the error that occurred on the
// stack, this will be popped and copied to the ErrorString field.

void process_error(objScript *Self, CSTRING Procedure)
{
   auto prv = (prvFluid *)Self->ChildPrivate;

   auto flags = VLF::WARNING;
   if (prv->CaughtError != ERR::Okay) {
      Self->Error = prv->CaughtError;
      if (Self->Error <= ERR::Terminate) flags = VLF::DETAIL; // Non-critical errors are muted to prevent log noise.
   }
   else Self->Error = ERR::Exception; // Unspecified exception, e.g. an error() or assert().  The result string will indicate detail.

   pf::Log log;
   CSTRING str = lua_tostring(prv->Lua, -1);
   lua_pop(prv->Lua, 1);  // pop returned value
   Self->setErrorString(str);

   CSTRING file = Self->Path;
   if (file) {
      int i;
      for (i=strlen(file); (i > 0) and (file[i-1] != '/') and (file[i-1] != '\\'); i--);
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

//********************************************************************************************************************
// This routine is intended for handling action notifications only.  It takes the FunctionField list provided by the
// action and copies them into a table.  Each value is represented by the relevant parameter name for ease of use.

static ERR stack_args(lua_State *Lua, OBJECTID ObjectID, const FunctionField *args, int8_t *Buffer)
{
   pf::Log log(__FUNCTION__);
   int j;

   if (not args) return ERR::Okay;

   log.traceBranch("Args: %p, Buffer: %p", args, Buffer);

   for (int i=0; args[i].Name; i++) {
      auto name = std::make_unique<char[]>(strlen(args[i].Name)+1);
      for (j=0; args[i].Name[j]; j++) name[j] = std::tolower(args[i].Name[j]);
      name[j] = 0;

      lua_pushlstring(Lua, name.get(), j);

      // Note: If the object is public and the call was messaged from a foreign process, all strings/pointers are
      // invalid because the message handlers cannot do deep pointer resolution of the structure we receive from
      // action notifications.

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
      else if (args[i].Type & FD_INT) {
         lua_pushinteger(Lua, ((int *)Buffer)[0]);
         Buffer += sizeof(int);
      }
      else if (args[i].Type & FD_DOUBLE) {
         lua_pushnumber(Lua, ((double *)Buffer)[0]);
         Buffer += sizeof(double);
      }
      else if (args[i].Type & FD_INT64) {
         lua_pushnumber(Lua, ((int64_t *)Buffer)[0]);
         Buffer += sizeof(int64_t);
      }
      else {
         log.warning("Unsupported arg %s, flags $%.8x, aborting now.", args[i].Name, args[i].Type);
         return ERR::Failed;
      }
      lua_settable(Lua, -3);
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// Action notifications arrive when the user has used object.subscribe() in the Fluid script.
//
// function(ObjectID, Args, Reference)

void notify_action(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   auto Self = (objScript *)CurrentContext();

   if (Result != ERR::Okay) return;

   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv) return;

   for (auto &scan : prv->ActionList) {
      if ((Object->UID IS scan.ObjectID) and (ActionID IS scan.ActionID)) {
         int depth = GetResource(RES::LOG_DEPTH); // Required because thrown errors cause the debugger to lose its branch

         {
            pf::Log log;

            log.msg(VLF::BRANCH|VLF::DETAIL, "Action notification for object #%d, action %d.  Top: %d", Object->UID, int(ActionID), lua_gettop(prv->Lua));

            lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, scan.Function); // +1 stack: Get the function reference
            push_object_id(prv->Lua, Object->UID);  // +1: Pass the object ID
            lua_newtable(prv->Lua);  // +1: Table to store the parameters

            if ((scan.Args) and (Args)) {
               stack_args(prv->Lua, Object->UID, scan.Args, (int8_t *)Args);
            }

            int total_args = 2;

            if (scan.Reference) { // +1: Custom reference (optional)
               lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, scan.Reference);
               total_args++; // ObjectID, ArgTable, Reference
            }

            if (lua_pcall(prv->Lua, total_args, 0, 0)) { // Make the call, function & args are removed from stack.
               process_error(Self, "Action Subscription");
            }

            log.traceBranch("Collecting garbage.");
            lua_gc(prv->Lua, LUA_GCCOLLECT, 0);
         }

         SetResource(RES::LOG_DEPTH, depth);
         return;
      }
   }
}

//********************************************************************************************************************

static ERR FLUID_Activate(objScript *Self)
{
   pf::Log log;

   if ((not Self->String) or (not Self->String[0])) return log.warning(ERR::FieldNotSet);

   log.trace("Target: %d, Procedure: %s / ID #%" PF64, Self->TargetID, Self->Procedure ? Self->Procedure : (STRING)".", (long long)Self->ProcedureID);

   ERR error = ERR::Failed;

   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv) return log.warning(ERR::ObjectCorrupt);

   if (prv->Recurse) { // When performing a recursive call, we can assume that the code has already been loaded.
      error = run_script(Self);
      if (error != ERR::Okay) Self->Error = error;

      {
         pf::Log log;
         log.traceBranch("Collecting garbage.");
         lua_gc(prv->Lua, LUA_GCCOLLECT, 0);
      }

      return ERR::Okay;
   }

   prv->Recurse++;

   Self->CurrentLine = -1;
   Self->Error       = ERR::Okay;

   bool reload = false;
   if (not Self->ActivationCount) reload = true;

   int i, j;
   if ((Self->ActivationCount) and (not Self->Procedure) and (not Self->ProcedureID)) {
      // If no procedure has been specified, kill the old Lua instance to restart from scratch

      free_all(Self);
      new (prv) prvFluid;

      if (not (prv->Lua = luaL_newstate())) {
         log.warning("Failed to open a Lua instance.");
         goto failure;
      }

      reload = true;
   }

   if (reload) {
      log.trace("The Lua script will be initialised from scratch.");

      prv->Lua->Script = Self;
      prv->Lua->ProtectedGlobals = false;

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

      if (register_interfaces(Self) != ERR::Okay) goto failure;

      // Line hook, executes on the execution of a new line

      if ((Self->Flags & SCF::LOG_ALL) != SCF::NIL) {
         // LUA_MASKLINE:  Interpreter is executing a line.
         // LUA_MASKCALL:  Interpreter is calling a function.
         // LUA_MASKRET:   Interpreter returns from a function.
         // LUA_MASKCOUNT: The hook will be called every X number of instructions executed.

         lua_sethook(prv->Lua, hook_debug, LUA_MASKCALL|LUA_MASKRET|LUA_MASKLINE, 0);
      }

      // Pre-load the Core module: mSys = mod.load('core')

      if (auto core = objModule::create::global(fl::Name("core"))) {
         SetName(core, "mSys");
         new_module(prv->Lua, core);
         lua_setglobal(prv->Lua, "mSys");
      }
      else {
         log.warning("Failed to create module object.");
         goto failure;
      }

      prv->Lua->ProtectedGlobals = true;

      // Determine chunk name for better debug output.
      // Prefix with '@' to indicate file-based chunk (Lua convention), otherwise use '=' for special sources.
      // This ensures debug output shows the actual filename instead of "[string]".
      std::string chunk_name;
      if (Self->Path) {
         chunk_name = std::string("@") + Self->Path;
      }
      else chunk_name = "=script";

      int result;
      if (startswith(LUA_COMPILED, Self->String)) { // The source is compiled
         log.trace("Loading pre-compiled Lua script.");
         int headerlen = strlen(Self->String) + 1;
         result = luaL_loadbuffer(prv->Lua, Self->String + headerlen, prv->LoadedSize - headerlen, chunk_name.c_str());
      }
      else {
         log.trace("Compiling Lua script.");
         result = luaL_loadbuffer(prv->Lua, Self->String, strlen(Self->String), chunk_name.c_str());
      }

      if (result) { // Error reported from parser
         if (auto errorstr = lua_tostring(prv->Lua,-1)) {
            // Format: [string "..."]:Line:Error
            if ((i = strsearch("\"]:", errorstr)) != -1) {
               i += 3;
               int line = strtol(errorstr + i, nullptr, 0);
               while ((errorstr[i]) and (errorstr[i] != ':')) i++;
               if (errorstr[i] IS ':') i++;

               std::ostringstream buf;
               buf << "Line " << line+Self->LineOffset << ": " << errorstr + i << '\n';
               CSTRING str = Self->String;

               for (j=1; j <= line+1; j++) {
                  if (j >= line-1) {
                     buf << (j + Self->LineOffset) << ": ";
                     int col;
                     for (col=0; (str[col]) and (str[col] != '\n') and (str[col] != '\r') and (col < 120); col++);
                     buf.write(str, col);
                     if (col IS 120) buf << "...";
                     buf << '\n';
                  }
                  if (not (str = next_line(str))) break;
               }
               Self->setErrorString(buf.str().c_str());

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

         prv->SaveCompiled = false;

         objFile::create cachefile = {
            fl::Path(Self->CacheFile), fl::Flags(FL::NEW|FL::WRITE), fl::Permissions(prv->CachePermissions)
         };

         if (cachefile.ok()) {
            save_binary(Self, *cachefile);
            cachefile->setDate(&prv->CacheDate);
         }
      }
   }
   else log.trace("Using the Lua script cache.");

   Self->ActivationCount++;

   if ((Self->Procedure) or (Self->ProcedureID)) {
      // The Lua script needs to have been executed at least once in order for the procedures to be initialised and recognised.

      if ((Self->ActivationCount IS 1) or (reload)) {
         pf::Log log;
         log.traceBranch("Collecting functions prior to procedure call...");

         if (lua_pcall(prv->Lua, 0, 0, 0)) {
            process_error(Self, "Activation");
         }
      }
   }

   if (Self->Error IS ERR::Okay) run_script(Self); // Will set Self->Error if there's an issue

   error = ERR::Okay; // The error reflects on the initial processing of the script only - the developer must check the Error field for information on script execution

failure:

   if (prv->Lua) {
      pf::Log log;
      log.traceBranch("Collecting garbage.");
      lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
   }

   prv->Recurse--;
   return error;
}

//********************************************************************************************************************

static ERR FLUID_DataFeed(objScript *Self, struct acDataFeed *Args)
{
   pf::Log log;

   if (not Args) return ERR::NullArgs;

   if (Args->Datatype IS DATA::TEXT) {
      Self->setStatement((CSTRING)Args->Buffer);
   }
   else if (Args->Datatype IS DATA::XML) {
      Self->setStatement((CSTRING)Args->Buffer);
   }
   else if (Args->Datatype IS DATA::RECEIPT) {
      auto prv = (prvFluid *)Self->ChildPrivate;

      log.branch("Incoming data receipt from #%d", Args->Object ? Args->Object->UID : 0);

      for (auto it = prv->Requests.begin(); it != prv->Requests.end(); ) {
         if ((Args->Object) and (it->SourceID IS Args->Object->UID)) {
            // Execute the callback associated with this input subscription: function({Items...})

            int step = GetResource(RES::LOG_DEPTH); // Required as thrown errors cause the debugger to lose its step position

               lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, it->Callback); // +1 Reference to callback
               lua_newtable(prv->Lua); // +1 Item table

               if (auto xml = objXML::create::local(fl::Statement((CSTRING)Args->Buffer))) {
                  // <file path="blah.exe"/> becomes { item='file', path='blah.exe' }

                  if (not xml->Tags.empty()) {
                     auto &tag = xml->Tags[0];
                     int i = 1;
                     if (iequals("receipt", tag.name())) {
                        for (auto &scan : tag.Children) {
                           lua_pushinteger(prv->Lua, i++);
                           lua_newtable(prv->Lua);

                           lua_pushstring(prv->Lua, "item");
                           lua_pushstring(prv->Lua, scan.name());
                           lua_settable(prv->Lua, -3);

                           for (unsigned a=1; a < scan.Attribs.size(); a++) {
                              lua_pushstring(prv->Lua, scan.Attribs[a].Name.c_str());
                              lua_pushstring(prv->Lua, scan.Attribs[a].Value.c_str());
                              lua_settable(prv->Lua, -3);
                           }

                           lua_settable(prv->Lua, -3);
                        }
                     }
                  }

                  FreeResource(xml);

                  if (lua_pcall(prv->Lua, 1, 0, 0)) { // function(Items)
                     process_error(Self, "Data Receipt Callback");
                  }
               }

            SetResource(RES::LOG_DEPTH, step);

            it = prv->Requests.erase(it);
            continue;
         }
         it++;
      }

      {
         pf::Log log;
         log.traceBranch("Collecting garbage.");
         lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR FLUID_DebugLog(objScript *Self, struct sc::DebugLog *Args)
{
   pf::Log log;

   if (not Args) return log.warning(ERR::NullArgs);
   
   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv->Lua) return log.warning(ERR::NotInitialised);

   log.branch("Options: %s", Args->Options ? Args->Options : "(none)");

   // Parse options (CSV list)

   bool show_stack = false;
   bool show_locals = false;
   bool show_upvalues = false;
   bool show_globals = false;
   bool show_memory = false;
   bool show_state = false;
   bool show_bytecode = false;
   bool show_funcinfo = false;
   bool compact = false;
   bool log_output = false;

   if (Args->Options) {
      std::string_view opts = Args->Options;

      if (opts.find("all") != std::string::npos) {
         show_stack = show_locals = show_upvalues = show_globals =
         show_memory = show_state = show_bytecode = true;
         show_funcinfo = true;
      }
      else {
         show_stack    = (opts.find("stack") != std::string::npos);
         show_locals   = (opts.find("locals") != std::string::npos);
         show_upvalues = (opts.find("upvalues") != std::string::npos);
         show_globals  = (opts.find("globals") != std::string::npos);
         show_memory   = (opts.find("memory") != std::string::npos);
         show_state    = (opts.find("state") != std::string::npos);
         show_bytecode = (opts.find("bytecode") != std::string::npos);
         show_funcinfo = (opts.find("funcinfo") != std::string::npos);
         log_output    = (opts.find("log") != std::string::npos);
      }
      compact = (opts.find("compact") != std::string::npos);
   }
   else show_stack = true; // Default: just the stack trace

   // Build the log message

   std::ostringstream buf;

   if (prv->Recurse) {
      // If Recurse is defined then we know what we're being called from within the script itself.
      // Here we can process options that are exclusive to internal calls.

      if (show_stack) { // Stack trace
         if (not compact) buf << "=== CALL STACK ===\n";

#if LJ_HASPROFILE
         size_t dump_len = 0;
         // Format codes: F=function name, l=source:line, p=preserve full path
         const char *dump = luaJIT_profile_dumpstack(prv->Lua, compact ? "pF (l)\n" : "l f\n", 50, &dump_len);
         if (dump and dump_len) {
            // Skip the first line (level 0) which is the C function mtDebugLog itself
            const char *first_newline = (const char *)memchr(dump, '\n', dump_len);
            if (first_newline and (first_newline - dump + 1) < dump_len) {
               const char *start = first_newline + 1;
               size_t len = dump_len - (start - dump);
               buf << std::string_view(start, len);
            }
         }
#else
         lua_Debug ar;
         int level = 1; // Start at 1 to skip the C function (mtDebugLog) itself

         while (lua_getstack(prv->Lua, level, &ar)) {
            lua_getinfo(prv->Lua, "nSl", &ar);

            if (compact) {
               buf << "[" << level << "] ";
               if (ar.name) buf << ar.name;
               else buf << "?";
               if (ar.source and ar.source[0]) buf << " (" << ar.short_src << ":" << ar.currentline << ")";
               buf << "\n";
            }
            else {
               buf << "[" << level << "] ";
               if (ar.name) buf << ar.name;
               else buf << "<anonymous>";

               if (ar.source and ar.source[0]) {
                  buf << " (" << ar.short_src << ":" << ar.currentline << ")";
               }

               buf << " - ";
               if (ar.what) {
                  if (strcmp(ar.what, "Lua") IS 0) buf << "Lua function";
                  else if (strcmp(ar.what, "C") IS 0) buf << "C function";
                  else if (strcmp(ar.what, "main") IS 0) buf << "main chunk";
                  else buf << ar.what;
               }
               buf << "\n";
            }
            level++;
         }
#endif

         if (not compact) buf << "\n";
      }

      if (show_funcinfo) {
         if (not compact) buf << "=== FUNCTION INFORMATION ===\n";

         int level = 1;
         bool wrote = false;
         lua_Debug ar;

         while (lua_getstack(prv->Lua, level, &ar)) {
            if (not lua_getinfo(prv->Lua, "nSl", &ar)) {
               level++;
               continue;
            }

            const char *func_name = ar.name ? ar.name : "<anonymous>";
            bool is_lua_func = false;
            int param_count = 0;
            bool is_vararg = false;
            uint32_t frame_slots = 0;
            uint32_t bytecodes = 0;
            uint32_t numeric_consts = 0;
            uint32_t object_consts = 0;
            int upvalues = 0;

            if (lua_getinfo(prv->Lua, "f", &ar)) {
               GCfunc *fn = funcV(prv->Lua->top - 1);
               upvalues = fn->c.nupvalues;
               if (isluafunc(fn)) {
                  is_lua_func = true;
                  GCproto *pt = funcproto(fn);
                  param_count = pt->numparams;
                  is_vararg = (pt->flags & PROTO_VARARG) != 0;
                  frame_slots = pt->framesize;
                  bytecodes = pt->sizebc;
                  numeric_consts = pt->sizekn;
                  object_consts = pt->sizekgc;
               }
               else is_vararg = true;
               lua_pop(prv->Lua, 1);
            }

            if (compact) {
               buf << "[" << level << "] " << func_name;
               if (is_lua_func) {
                  buf << " (" << ar.short_src << ":" << ar.linedefined << "-" << ar.lastlinedefined << ")";
                  buf << " params=" << param_count;
                  buf << " vararg=" << (is_vararg ? "true" : "false");
                  buf << " slots=" << frame_slots;
                  buf << " bytecode=" << bytecodes;
                  buf << " consts=" << numeric_consts << "+" << object_consts;
               }
               else buf << " <C function>";
               buf << "\n";
            }
            else {
               buf << "Function [" << level << "]: " << func_name;
               if (is_lua_func) {
                  buf << " (" << ar.short_src << ":" << ar.linedefined << "-" << ar.lastlinedefined << ")\n";
                  buf << "   Parameters: " << param_count << "\n";
                  buf << "   Vararg: " << (is_vararg ? "true" : "false") << "\n";
                  buf << "   Stack slots: " << frame_slots << "\n";
                  buf << "   Bytecodes: " << bytecodes << "\n";
                  buf << "   Constants: " << numeric_consts << " numeric, " << object_consts << " objects\n";
                  buf << "   Upvalues: " << upvalues << "\n";
               }
               else {
                  buf << " (<C function>)\n";
                  buf << "   Upvalues: " << upvalues << "\n";
               }
            }

            wrote = true;
            level++;
         }

         if (not wrote) buf << "(no frames)";
         if (not compact) buf << "\n";
      }

      if (show_locals) { // Local variables
         lua_Debug ar;
         if (lua_getstack(prv->Lua, 1, &ar)) { // Level 1 = caller's frame
            if (not compact) buf << "=== LOCALS ===\n";

            int idx = 1;
            const char *name;
            while ((name = lua_getlocal(prv->Lua, &ar, idx))) {
               int type = lua_type(prv->Lua, -1);
               buf << name << " = ";

               switch (type) {
                  case LUA_TNIL: buf << "nil"; break;
                  case LUA_TBOOLEAN: buf << (lua_toboolean(prv->Lua, -1) ? "true" : "false"); break;
                  case LUA_TNUMBER: buf << lua_tonumber(prv->Lua, -1); break;
                  case LUA_TSTRING: {
                     size_t len;
                     const char *str = lua_tolstring(prv->Lua, -1, &len);
                     std::string_view sv(str, len);
                     if (len > 40) buf << "\"" << sv.substr(0, 40) << "...\"";
                     else buf << "\"" << sv << "\"";
                     break;
                  }
                  case LUA_TTABLE: buf << "{ ... }"; break;
                  case LUA_TFUNCTION: buf << "<function>"; break;
                  case LUA_TUSERDATA: buf << "<userdata>"; break;
                  case LUA_TTHREAD: buf << "<thread>"; break;
                  default: buf << "<" << lua_typename(prv->Lua, type) << ">"; break;
               }

               if (not compact) buf << " (" << lua_typename(prv->Lua, type) << ")";
               buf << "\n";

               lua_pop(prv->Lua, 1);
               idx++;
            }

            if (not compact) buf << "\n";
         }
      }

      if (show_upvalues) { // Upvalues
         lua_Debug ar;
         if (lua_getstack(prv->Lua, 1, &ar)) { // Level 1 = caller's frame
            lua_getinfo(prv->Lua, "f", &ar);

            if (not compact) buf << "=== UPVALUES ===\n";

            int idx = 1;
            const char *name;
            while ((name = lua_getupvalue(prv->Lua, -1, idx))) {
               int type = lua_type(prv->Lua, -1);

               buf << name << " = ";

               switch (type) {
                  case LUA_TNIL: buf << "nil"; break;
                  case LUA_TBOOLEAN: buf << (lua_toboolean(prv->Lua, -1) ? "true" : "false"); break;
                  case LUA_TNUMBER: buf << lua_tonumber(prv->Lua, -1); break;
                  case LUA_TSTRING: {
                     size_t len;
                     const char *str = lua_tolstring(prv->Lua, -1, &len);
                     std::string_view sv(str, len);
                     if (len > 40) buf << "\"" << sv.substr(0, 40) << "...\"";
                     else buf << "\"" << sv << "\"";
                     break;
                  }
                  case LUA_TTABLE: buf << "{ ... }"; break;
                  case LUA_TFUNCTION: buf << "<function>"; break;
                  default: buf << "<" << lua_typename(prv->Lua, type) << ">"; break;
               }

               if (not compact) buf << " (" << lua_typename(prv->Lua, type) << ")";
               buf << "\n";
               lua_pop(prv->Lua, 1);
               idx++;
            }

            lua_pop(prv->Lua, 1); // Pop the function
            if (not compact) buf << "\n";
         }
      }
   }

   // Here we can process options that are meaningful post-execution.
   
   // Bytecode (if available via jit.bc)
   if (show_bytecode) {
      if (not compact) buf << "=== BYTECODE ===\n";
      buf << "(Bytecode inspection requires jit.bc module - use require('jit.bc').dump(func))\n";
      if (not compact) buf << "\n";
   }

   if (show_globals) { // Global variables
      if (not compact) buf << "=== GLOBALS ===\n";

      // Access the storage table where user-defined globals are stored.
      // The storage table is stored as an upvalue in the __index closure of the global metatable.

      lua_pushvalue(prv->Lua, LUA_GLOBALSINDEX); // Push global environment
      if (lua_getmetatable(prv->Lua, -1)) {
         lua_pushstring(prv->Lua, "__index");
         lua_rawget(prv->Lua, -2); // Get the __index closure

         if (lua_isfunction(prv->Lua, -1)) {
            // The storage table is the first upvalue of the __index closure
            auto upvalue_name = lua_getupvalue(prv->Lua, -1, 1);
            if (upvalue_name and lua_istable(prv->Lua, -1)) {
               int count = 0;
               lua_pushnil(prv->Lua);
               while (lua_next(prv->Lua, -2)) {
                  const char *key = lua_tostring(prv->Lua, -2);
                  buf << key << " = ";

                  int type = lua_type(prv->Lua, -1);
                  switch (type) {
                     case LUA_TNIL: buf << "nil"; break;
                     case LUA_TBOOLEAN: buf << (lua_toboolean(prv->Lua, -1) ? "true" : "false"); break;
                     case LUA_TNUMBER: buf << lua_tonumber(prv->Lua, -1); break;
                     case LUA_TSTRING: {
                        size_t len;
                        const char *str = lua_tolstring(prv->Lua, -1, &len);
                        std::string_view sv(str, len);
                        if (len > 40) buf << "\"" << sv.substr(0, 40) << "...\"";
                        else buf << "\"" << sv << "\"";
                        break;
                     }
                     case LUA_TTABLE: buf << "{ ... }"; break;
                     case LUA_TFUNCTION: buf << "<function>"; break;
                     default: buf << "<" << lua_typename(prv->Lua, type) << ">"; break;
                  }

                  if (not compact) buf << " (" << lua_typename(prv->Lua, type) << ")";
                  buf << "\n";
                  count++;

                  lua_pop(prv->Lua, 1);
               }

               if (count IS 0) buf << "(none)\n";
               lua_pop(prv->Lua, 1); // Pop storage table
            }
         }
         lua_pop(prv->Lua, 1); // Pop __index closure
         lua_pop(prv->Lua, 1); // Pop metatable
      }
      lua_pop(prv->Lua, 1); // Pop global environment

      if (not compact) buf << "\n";
   }

   if (show_memory) { // Memory statistics
      if (not compact) buf << "=== MEMORY STATISTICS ===\n";

      int kb = lua_gc(prv->Lua, LUA_GCCOUNT, 0);
      int bytes = lua_gc(prv->Lua, LUA_GCCOUNTB, 0);
      double mb = kb / 1024.0 + bytes / (1024.0 * 1024.0);

      if (compact) buf << "Lua heap: " << mb << " MB\n";
      else buf << "Lua heap usage: " << mb << " MB (" << kb << " KB + " << bytes << " bytes)\n";

      if (not compact) buf << "\n";
   }
  
   if (show_state) { // State information
      if (not compact) buf << "=== STATE ===\n";

      buf << "Stack top: " << lua_gettop(prv->Lua) << "\n";
      buf << "Protected globals: " << (prv->Lua->ProtectedGlobals ? "true" : "false") << "\n";

      if (auto hook_mask = lua_gethookmask(prv->Lua)) {
         buf << "Hook mask: ";
         bool first = true;
         if (hook_mask & LUA_MASKCALL) { buf << (first ? "" : "|") << "CALL"; first = false; }
         if (hook_mask & LUA_MASKRET) { buf << (first ? "" : "|") << "RET"; first = false; }
         if (hook_mask & LUA_MASKLINE) { buf << (first ? "" : "|") << "LINE"; first = false; }
         if (hook_mask & LUA_MASKCOUNT) { buf << (first ? "" : "|") << "COUNT"; first = false; }
         buf << "\n";
      }
      else buf << "Hook mask: none\n";

      if (not compact) buf << "\n";
   }

   std::string result = buf.str();
   if ((Args->Result = pf::strclone(result.c_str())) == nullptr) {
      return ERR::AllocMemory;
   }
   else {
      if (log_output) log.msg("%.400s", Args->Result);
      return ERR::Okay;
   }
}

//********************************************************************************************************************

static ERR FLUID_DerefProcedure(objScript *Self, struct sc::DerefProcedure *Args)
{
   pf::Log log;

   if (not Args) return ERR::NullArgs;

   if ((Args->Procedure) and (Args->Procedure->isScript())) {
      if (Args->Procedure->Context IS Self) { // Verification of ownership
         auto prv = (prvFluid *)Self->ChildPrivate;
         if (not prv) return log.warning(ERR::ObjectCorrupt);

         log.trace("Dereferencing procedure #%" PF64, (long long)Args->Procedure->ProcedureID);

         if (Args->Procedure->ProcedureID) {
            luaL_unref(prv->Lua, LUA_REGISTRYINDEX, Args->Procedure->ProcedureID);
            Args->Procedure->ProcedureID = 0;
         }
         return ERR::Okay;
      }
      else return log.warning(ERR::Args);
   }
   else return log.warning(ERR::Args);
}

//********************************************************************************************************************

static ERR FLUID_Free(objScript *Self)
{
   free_all(Self);
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR FLUID_GetProcedureID(objScript *Self, struct sc::GetProcedureID *Args)
{
   pf::Log log;

   if ((not Args) or (not Args->Procedure) or (not Args->Procedure[0])) return log.warning(ERR::NullArgs);

   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv) return log.warning(ERR::ObjectCorrupt);

   if ((not prv->Lua) or (not Self->ActivationCount)) {
      log.warning("Cannot resolve function '%s'.  Script requires activation.", Args->Procedure);
      return ERR::NotFound;
   }

   lua_getglobal(prv->Lua, Args->Procedure);
   auto id = luaL_ref(prv->Lua, LUA_REGISTRYINDEX);
   if ((id != LUA_REFNIL) and (id != LUA_NOREF)) {
      Args->ProcedureID = id;
      return ERR::Okay;
   }
   else {
      log.warning("Failed to resolve function name '%s' to an ID.", Args->Procedure);
      return ERR::NotFound;
   }
}

//********************************************************************************************************************

static ERR FLUID_Init(objScript *Self)
{
   pf::Log log;

   if (Self->Path) {
      if (not wildcmp("*.fluid|*.fb|*.lua", Self->Path)) {
         log.warning("No support for path '%s'", Self->Path);
         return ERR::NoSupport;
      }
   }

   if ((Self->defined(NF::RECLASSED)) and (not Self->String)) {
      log.trace("No support for reclassed Script with no String field value.");
      return ERR::NoSupport;
   }

   STRING str;
   ERR error;
   bool compile = false;
   int loaded_size = 0;
   objFile *src_file = nullptr;
   if ((not Self->String) and (Self->Path)) {
      int64_t src_ts = 0, src_size = 0;

      if ((src_file = objFile::create::local(fl::Path(Self->Path)))) {
         error = src_file->get(FID_TimeStamp, src_ts);
         if (error IS ERR::Okay) error = src_file->get(FID_Size, src_size);
      }
      else error = ERR::File;

      if (Self->CacheFile) {
         // Compare the cache file date to the original source.  If they match, or if there was a problem
         // analysing the original location (i.e. the original location does not exist) then the cache file is loaded
         // instead of the original source code.

         int64_t cache_ts = -1, cache_size;

         {
            objFile::create cache_file = { fl::Path(Self->CacheFile) };
            if (cache_file.ok()) {
               cache_file->get(FID_TimeStamp, cache_ts);
               cache_file->get(FID_Size, cache_size);
            }
         }

         if (cache_ts != -1) {
            if ((cache_ts IS src_ts) or (error != ERR::Okay)) {
               log.msg("Using cache '%s'", Self->CacheFile);
               if (AllocMemory(cache_size, MEM::STRING|MEM::NO_CLEAR, &Self->String) IS ERR::Okay) {
                  int len;
                  error = ReadFileToBuffer(Self->CacheFile, Self->String, cache_size, &len);
                  loaded_size = cache_size;
               }
               else error = ERR::AllocMemory;
            }
         }
      }

      if ((error IS ERR::Okay) and (not loaded_size)) {
         if (AllocMemory(src_size+1, MEM::STRING|MEM::NO_CLEAR, &Self->String) IS ERR::Okay) {
            int len;
            if (ReadFileToBuffer(Self->Path, Self->String, src_size, &len) IS ERR::Okay) {
               Self->String[len] = 0;

               // Unicode BOM handler - in case the file starts with a BOM header.
               CSTRING bomptr = check_bom(Self->String);
               if (bomptr != Self->String) copymem(bomptr, Self->String, (len + 1) - (bomptr - Self->String));

               loaded_size = len;

               if (Self->CacheFile) compile = true; // Saving a compilation of the source is desired
            }
            else {
               log.trace("Failed to read %" PF64 " bytes from '%s'", (long long)src_size, Self->Path);
               FreeResource(Self->String);
               Self->String = nullptr;
               error = ERR::ReadFileToBuffer;
            }
         }
         else error = ERR::AllocMemory;
      }
   }
   else error = ERR::Okay;

   // Allocate private structure

   prvFluid *prv;
   if (error IS ERR::Okay) {
      if (AllocMemory(sizeof(prvFluid), MEM::DATA, &Self->ChildPrivate) IS ERR::Okay) {
         prv = (prvFluid *)Self->ChildPrivate;
         new (prv) prvFluid;
         if ((prv->SaveCompiled = compile)) {
            DateTime *dt;
            if (src_file->get(FID_Date, dt) IS ERR::Okay) prv->CacheDate = *dt;
            src_file->get(FID_Permissions, (int &)prv->CachePermissions);
            prv->LoadedSize = loaded_size;
         }
      }
      else error = ERR::AllocMemory;
   }

   if (error != ERR::Okay) {
      if (src_file) FreeResource(src_file);
      return log.warning(error);
   }

   log.trace("Opening a Lua instance.");

   if (not (prv->Lua = luaL_newstate())) {
      log.warning("Failed to open a Lua instance.");
      FreeResource(Self->ChildPrivate);
      Self->ChildPrivate = nullptr;
      if (src_file) FreeResource(src_file);
      return ERR::Failed;
   }

   if (not (str = Self->String)) {
      log.trace("No statement specified at this stage.");
      if (src_file) FreeResource(src_file);
      return ERR::Okay; // Assume that the script's text will be incoming later
   }

   // Search for a $FLUID comment - this can contain extra details and options for the script.  Valid identifiers are
   //
   //    -- $FLUID
   //    \* $FLUID
   //    // $FLUID

   if (wildcmp("?? $FLUID", str)) {

   }

   if (src_file) FreeResource(src_file);
   return ERR::Okay;
}

//********************************************************************************************************************
// If the script is being executed, retarget the new resource to refer to the current task (because we don't want
// client resources allocated by the script to be automatically destroyed when the script is terminated by the client).

static ERR FLUID_NewChild(objScript *Self, struct acNewChild &Args)
{
   auto prv = (prvFluid*)Self->ChildPrivate;
   if (not prv) return ERR::Okay;

   if (prv->Recurse) {
      SetOwner(Args.Object, CurrentTask());
      return ERR::OwnerPassThrough;
   }
   else return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
SaveToObject: Compiles the current script statement and saves it as byte code.

Use the SaveToObject action to compile the statement in the Script's String field and save the resulting byte code to a
target object.  The byte code can be loaded into any script object for execution or referenced in the Fluid code for
usage.

*********************************************************************************************************************/

static ERR FLUID_SaveToObject(objScript *Self, struct acSaveToObject *Args)
{
   pf::Log log;

   if ((not Args) or (not Args->Dest)) return log.warning(ERR::NullArgs);

   if (not Self->String) return log.warning(ERR::FieldNotSet);

   log.branch("Compiling the statement...");

   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv) return log.warning(ERR::ObjectCorrupt);

   std::string chunk_name;
   if (Self->Path) chunk_name = std::string("@") + Self->Path;
   else chunk_name = "=script";

   if (not luaL_loadbuffer(prv->Lua, Self->String, strlen(Self->String), chunk_name.c_str())) {
      ERR error = save_binary(Self, Args->Dest);
      return error;
   }
   else {
      auto str = lua_tostring(prv->Lua,-1);
      lua_pop(prv->Lua, 1);
      log.warning("Compile Failure: %s", str);
      return ERR::InvalidData;
   }
}

/*********************************************************************************************************************

-FIELD-
Procedures: Returns a string array of all named procedures defined by a script.

This field will return a string array of all procedures loaded into the script, conditional on it being activated.
It will otherwise return an empty array.
-END-

*********************************************************************************************************************/

static ERR GET_Procedures(objScript *Self, pf::vector<std::string> **Value, int *Elements)
{
   if (auto prv = (prvFluid *)Self->ChildPrivate) {
      prv->Procedures.clear();
      lua_pushnil(prv->Lua);
      while (lua_next(prv->Lua, LUA_GLOBALSINDEX)) {
         if (lua_type(prv->Lua, -1) IS LUA_TFUNCTION) {
            prv->Procedures.push_back(lua_tostring(prv->Lua, -2));
         }
         lua_pop(prv->Lua, 1);
      }

      *Value = &prv->Procedures;
      *Elements = prv->Procedures.size();
      return ERR::Okay;
   }
   else return ERR::NotInitialised;
}

//********************************************************************************************************************

static ERR save_binary(objScript *Self, OBJECTPTR Target)
{
   // TODO No support for save_binary() yet.

   return ERR::NoSupport;
/*
   pf::Log log(__FUNCTION__);
   prvFluid *prv;
   OBJECTPTR dest;
   const Proto *f;
   int i;

   log.branch("Save Symbols: %d", Self->Flags & SCF::LOG_ALL);

   if (not (prv = Self->ChildPrivate)) return LogReturnError(0, ERR::ObjectCorrupt);

   f = clvalue(prv->Lua->top + (-1))->l.p;

   // Write the fluid header first.  This must identify the content as compiled, plus include any relevant options,
   // such as the persistent identifier.

   if (not AccessObject(FileID, 3000, &dest)) {
      int result;
      UBYTE header[256];

      i = StrCopy(LUA_COMPILED, header, sizeof(header));
      header[i++] = 0;

      if (acWrite(dest, header, i, &result)) {
         ReleaseObject(dest);
         return LogReturnError(0, ERR::Write);
      }
      ReleaseObject(dest);
   }

   if ((code_writer_id > 0) and ((dest = GetObjectPtr(FileID)))) {
      luaU_dump(prv->Lua, f, &code_writer, dest, (Self->Flags & SCF::LOG_ALL) ? 0 : 1);
   }
   else luaU_dump(prv->Lua, f, &code_writer_id, (void *)(MAXINT)FileID, (Self->Flags & SCF::LOG_ALL) ? 0 : 1);

   LogReturn();
   return ERR::Okay;
*/
}

//********************************************************************************************************************

static ERR run_script(objScript *Self)
{
   pf::Log log(__FUNCTION__);

   auto prv = (prvFluid *)Self->ChildPrivate;

   log.traceBranch("Procedure: %s, Top: %d", Self->Procedure, lua_gettop(prv->Lua));

   prv->CaughtError = ERR::Okay;
   struct object * release_list[8];
   int r = 0;
   int top;
   int pcall_failed = false;
   if ((Self->Procedure) or (Self->ProcedureID)) {
      if (Self->Procedure) lua_getglobal(prv->Lua, Self->Procedure);
      else lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, Self->ProcedureID);

      if (lua_isfunction(prv->Lua, -1)) {
         if ((Self->Flags & SCF::LOG_ALL) != SCF::NIL) log.branch("Executing procedure: %s, Args: %d", Self->Procedure, Self->TotalArgs);

         top = lua_gettop(prv->Lua);

         int count = 0;
         const ScriptArg *args;
         if ((args = Self->ProcArgs)) {
            for (int i=0; i < Self->TotalArgs; i++, args++) {
               int type = args->Type;

               if (type & FD_ARRAY) {
                  log.trace("Setting arg '%s', Array: %p", args->Name, args->Address);

                  APTR values = args->Address;
                  int total_elements = -1;
                  CSTRING arg_name = args->Name;
                  if (args[1].Type & FD_ARRAYSIZE) {
                     if (args[1].Type & FD_INT) total_elements = args[1].Int;
                     else if (args[1].Type & FD_INT64) total_elements = args[1].Int64;
                     else values = nullptr;
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
                     if (named_struct_to_table(prv->Lua, args->Name, args->Address) != ERR::Okay) lua_pushnil(prv->Lua);
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
                     if (args[1].Type & FD_INT) make_array(prv->Lua, FD_BYTE|FD_WRITE, nullptr, (APTR *)args->Address, args[1].Int, false);
                     else if (args[1].Type & FD_INT64) make_array(prv->Lua, FD_BYTE|FD_WRITE, nullptr, (APTR *)args->Address, args[1].Int64, false);
                     else lua_pushnil(prv->Lua);
                     i++; args++; // Because we took the buffer-size parameter into account
                  }
                  else if (type & FD_OBJECT) {
                     // Pushing direct object pointers is considered safe because they are treated as detached, then
                     // a lock is gained for the duration of the call that is then released on return.  This is a
                     // solid optimisation that also protects the object from unwarranted termination during the call.

                     if (args->Address) {
                        struct object *obj = push_object(prv->Lua, (OBJECTPTR)args->Address);
                        if ((r < std::ssize(release_list)) and (access_object(obj))) {
                           release_list[r++] = obj;
                        }
                     }
                     else lua_pushnil(prv->Lua);
                  }
                  else lua_pushlightuserdata(prv->Lua, args->Address);
               }
               else if (type & FD_INT)   {
                  log.trace("Setting arg '%s', Value: %d", args->Name, args->Int);
                  if (type & FD_OBJECT) {
                     if (args->Int) push_object_id(prv->Lua, args->Int);
                     else lua_pushnil(prv->Lua);
                  }
                  else lua_pushinteger(prv->Lua, args->Int);
               }
               else if (type & FD_INT64)  { log.trace("Setting arg '%s', Value: %" PF64, args->Name, (long long)args->Int64); lua_pushnumber(prv->Lua, args->Int64); }
               else if (type & FD_DOUBLE) { log.trace("Setting arg '%s', Value: %.2f", args->Name, args->Double); lua_pushnumber(prv->Lua, args->Double); }
               else { lua_pushnil(prv->Lua); log.warning("Arg '%s' uses unrecognised type $%.8x", args->Name, type); }
               count++;
            }
         }

         int step = GetResource(RES::LOG_DEPTH);

         if (lua_pcall(prv->Lua, count, LUA_MULTRET, 0)) {
            pcall_failed = true;
         }

         SetResource(RES::LOG_DEPTH, step);

         while (r > 0) release_object(release_list[--r]);
      }
      else {
         std::ostringstream ss;
         ss << "Procedure '" << (Self->Procedure ? Self->Procedure : "NULL") << "' / #" << Self->ProcedureID << " does not exist in the script.";
         auto str = ss.str().c_str();
         Self->setErrorString(str);
         log.warning("%s", str);

         #ifdef _DEBUG
            pf::vector<std::string> *list;
            int total_procedures;
            if (GET_Procedures(Self, &list, &total_procedures) IS ERR::Okay) {
               for (int i=0; i < total_procedures; i++) log.trace("%s", list[0][i]);
            }
         #endif

         Self->Error = ERR::NotFound;
         return ERR::NotFound;
      }
   }
   else {
      int depth = GetResource(RES::LOG_DEPTH);

         top = lua_gettop(prv->Lua);
         if (lua_pcall(prv->Lua, 0, LUA_MULTRET, 0)) {
            pcall_failed = true;
         }

      SetResource(RES::LOG_DEPTH, depth);
   }

   if (not pcall_failed) { // If the procedure returned results, copy them to the Results field of the Script.
      int results = lua_gettop(prv->Lua) - top + 1;

      if (results > 0) {
         std::vector<CSTRING> array;
         array.resize(results+1);

         // NB: The Results field will take a clone of the Lua strings, so this sub-routine is safe to pass
         // on Lua's temporary string results.

         int i;
         for (i=0; i < results; i++) {
            array[i] = lua_tostring(prv->Lua, -results+i);
            log.trace("Result: %d/%d: %s", i, results, array[i]);
         }
         array[i] = nullptr;
         Self->set(FID_Results, array.data(), i);
         lua_pop(prv->Lua, results);  // pop returned values
      }

      return ERR::Okay;
   }
   else {
      // LuaJIT catches C++ exceptions, but we would prefer that crashes occur normally so that they can be traced in
      // the debugger.  As we don't have a solution to this design issue yet, the following context check will suffice
      // to prevent unwanted behaviour.

      if (CurrentContext() != Self) abort(); // A C++ exception was caught by Lua - the software stack is unstable so we must abort.

      process_error(Self, Self->Procedure ? Self->Procedure : "run_script");
      return Self->Error;
   }
}

//********************************************************************************************************************

static ERR register_interfaces(objScript *Self)
{
   pf::Log log;

   log.traceBranch("Registering Parasol and Fluid interfaces with Lua.");

   auto prv = (prvFluid *)Self->ChildPrivate;

   register_array_class(prv->Lua);
   register_io_class(prv->Lua);
   register_object_class(prv->Lua);
   register_module_class(prv->Lua);
   register_regex_class(prv->Lua);
   register_struct_class(prv->Lua);
   register_thread_class(prv->Lua);
   #ifndef DISABLE_DISPLAY
      register_input_class(prv->Lua);
   #endif
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

   return ERR::Okay;
}

//********************************************************************************************************************

ERR create_fluid(void)
{
   clFluid = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::SCRIPT),
      fl::ClassID(CLASSID::FLUID),
      fl::ClassVersion(1.0),
      fl::Name("Fluid"),
      fl::Category(CCF::DATA),
      fl::FileExtension("*.fluid|*.fb|*.lua"),
      fl::FileDescription("Fluid"),
      fl::Actions(clActions),
      fl::Methods(clMethods),
      fl::Fields(clFields),
      fl::Path(MOD_PATH));

   return clFluid ? ERR::Okay : ERR::AddClass;
}