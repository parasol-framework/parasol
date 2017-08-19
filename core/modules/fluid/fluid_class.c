
// Dump the variables of any global table

static ERROR register_interfaces(objScript *) __attribute__ ((unused));
static void dump_global_table(objScript *, STRING Global) __attribute__ ((unused));

static void dump_global_table(objScript *Self, STRING Global)
{
   lua_State *lua = ((struct prvFluid *)Self->Head.ChildPrivate)->Lua;
   lua_getglobal(lua, Global);
   if (lua_istable(lua, -1) ) {
      lua_pushnil(lua);
      while (lua_next(lua, -2) != 0) {
         LONG type = lua_type(lua, -2);
         LogF("print_env:","%s = %s", lua_tostring(lua, -2), lua_typename(lua, type));
         lua_pop(lua, 1);
      }
   }
}

//****************************************************************************

static ERROR GET_Procedures(objScript *, STRING **, LONG *);

static const struct FieldArray clFields[] = {
   { "Procedures", FDF_VIRTUAL|FDF_ARRAY|FDF_STRING|FDF_ALLOC|FDF_R, 0, GET_Procedures, NULL },
   END_FIELD
};

//****************************************************************************

static ERROR FLUID_ActionNotify(objScript *, struct acActionNotify *);
static ERROR FLUID_Activate(objScript *, APTR);
static ERROR FLUID_DataFeed(objScript *, struct acDataFeed *);
static ERROR FLUID_Free(objScript *, APTR);
static ERROR FLUID_Init(objScript *, APTR);
static ERROR FLUID_SaveToObject(objScript *, struct acSaveToObject *);

static const struct ActionArray clActions[] = {
   { AC_ActionNotify, FLUID_ActionNotify },
   { AC_Activate,     FLUID_Activate },
   { AC_DataFeed,     FLUID_DataFeed },
   { AC_Free,         FLUID_Free },
   { AC_Init,         FLUID_Init },
   { AC_SaveToObject, FLUID_SaveToObject },
   { 0, NULL }
};

//****************************************************************************

static ERROR FLUID_GetProcedureID(objScript *, struct scGetProcedureID *);
static ERROR FLUID_DerefProcedure(objScript *, struct scDerefProcedure *);

static const struct MethodArray clMethods[] = {
   { MT_ScGetProcedureID, FLUID_GetProcedureID, "GetProcedureID", NULL, 0 },
   { MT_ScDerefProcedure, FLUID_DerefProcedure, "DerefProcedure", NULL, 0 },
   { 0, NULL, NULL, NULL, 0 }
};


//****************************************************************************

static void free_all(objScript *Self)
{
   struct prvFluid *prv;

   if (!(prv = Self->Head.ChildPrivate)) return; // Not a problem - indicates the object did not pass initialisation

   clear_subscriptions(Self);

   if (prv->Structs) { VarFree(prv->Structs); prv->Structs = NULL; }
   if (prv->Includes) { VarFree(prv->Includes); prv->Includes = NULL; }
   if (prv->FocusEventHandle) { UnsubscribeEvent(prv->FocusEventHandle); prv->FocusEventHandle = NULL; }

   LogF("~7","Closing Lua instance %p.", prv->Lua);

      lua_close(prv->Lua);
      prv->Lua = NULL;

   LogBack();
}

//****************************************************************************
// Only to be used immediately after a failed lua_pcall().  Lua stores a description of the error that occurred on the
// stack, this will be popped and copied to the ErrorString field.

static void process_error(objScript *Self, CSTRING Procedure)
{
   struct prvFluid *prv = Self->Head.ChildPrivate;

   CSTRING header = "@";
   if (prv->CaughtError) {
      Self->Error = prv->CaughtError;
      if (Self->Error <= ERR_Terminate) header = "7"; // Non-critical errors are kept silent to prevent noise.
   }

   CSTRING str = lua_tostring(prv->Lua, -1);
   lua_pop(prv->Lua, 1);  // pop returned value
   SetString(Self, FID_ErrorString, str);

   CSTRING file = Self->Path;
   if (file) {
      LONG i = StrLength(file);
      for (i=StrLength(file); (i > 0) AND (file[i-1] != '/') AND (file[i-1] != '\\'); i--);
      LogF(header, "%s: %s", file+i, str);
   }
   else LogF(header, "%s: Error: %s", Procedure, str);

   // NB: CurrentLine is set by hook_debug(), so if debugging isn't active, you don't know what line we're on.

   if (Self->CurrentLine >= 0) {
      UBYTE line[60];
      get_line(Self, Self->CurrentLine, line, sizeof(line));
      LogF(header, "Line %d: %s...", Self->CurrentLine+1+Self->LineOffset, line);
   }
}

/*****************************************************************************
** This routine is intended for handling ActionNotify() messages only.  It takes the FunctionField list provided by the
** action and copies them into a table.  Each value is represented by the relevant parameter name for ease of use.
*/

static ERROR stack_args(lua_State *Lua, OBJECTID ObjectID, const struct FunctionField *args, APTR Buffer)
{
   LONG i, j;

   if (!args) return ERR_Okay;

   FMSG("~stack_args()","Args: %p, Buffer: %p", args, Buffer);

   for (i=0; args[i].Name; i++) {
      UBYTE name[StrLength(args[i].Name)+1];
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
         LogF("@stack_args","Unsupported arg %s, flags $%.8x, aborting now.", args[i].Name, args[i].Type);
         STEP();
         return ERR_Failed;
      }
      lua_settable(Lua, -3);
   }

   STEP();
   return ERR_Okay;
}

//****************************************************************************
// Action notifications arrive when the user has used object.subscribe() in the Fluid script.

static ERROR FLUID_ActionNotify(objScript *Self, struct acActionNotify *Args)
{
   if (!Args) return ERR_NullArgs;
   if (Args->Error != ERR_Okay) return ERR_Okay;

   struct prvFluid *prv = Self->Head.ChildPrivate;
   if (!prv) return ERR_Okay;

   struct actionmonitor *scan;
   for (scan=prv->ActionList; scan; scan=scan->Next) {
      if ((Args->ObjectID IS scan->ObjectID) AND (Args->ActionID IS scan->ActionID)) {
         LONG depth = GetResource(RES_LOG_DEPTH); // Required because thrown errors cause the debugger to lose its branch

         LogF("~7","Action notification for object #%d, action %d.  Top: %d", Args->ObjectID, Args->ActionID, lua_gettop(prv->Lua));

            lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, scan->Function); // +1 stack: Get the function reference
            push_object_id(prv->Lua, Args->ObjectID);  // +1: Pass the object ID
            lua_newtable(prv->Lua);  // +1: Table to store the parameters
            if (!stack_args(prv->Lua, Args->ObjectID, scan->Args, Args->Args)) {
               LONG total_args;
               if (scan->Reference) { // +1: Custom reference (optional)
                  lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, scan->Reference);
                  total_args = 3; // ObjectID, ArgTable, Reference
               }
               else total_args = 2; // ObjectID, ArgTable

               if (lua_pcall(prv->Lua, total_args, 0, 0)) { // Make the call, function & args are removed from stack.
                  process_error(Self, "Action Subscription");
               }

               LogF("~7","Collecting garbage.");
                 lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
               LogBack();
            }

         LogBack();

         SetResource(RES_LOG_DEPTH, depth);
         return ERR_Okay;
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR FLUID_Activate(objScript *Self, APTR Void)
{
   if (CurrentTaskID() != Self->Head.TaskID) return LogCode(ERR_IllegalActionAttempt);

   if ((!Self->String) OR (!Self->String[0])) return LogCode(ERR_FieldNotSet);

   LogF("~7","Target: %d, Procedure: %s / ID #" PF64(), Self->TargetID, Self->Procedure ? Self->Procedure : (STRING)".", Self->ProcedureID);

   ERROR error = ERR_Failed;

   struct prvFluid *prv;
   if (!(prv = Self->Head.ChildPrivate)) return LogCode(ERR_ObjectCorrupt);

   if (prv->Recurse) { // When performing a recursive call, we can assume that the code has already been loaded.
      error = run_script(Self);
      if (error) Self->Error = error;

      FMSG("~","Collecting garbage.");
         lua_gc(prv->Lua, LUA_GCCOLLECT, 0);
      STEP();
      LogBack();
      return ERR_Okay;
   }

   prv->Recurse++;

   Self->CurrentLine     = -1;
   Self->Error           = ERR_Okay;

   // Set the script owner to the current process, prior to script execution.  Once complete, we will change back to
   // the original owner.

   Self->ScriptOwnerID = Self->Head.OwnerID;
   OBJECTID owner_id = GetOwner(Self);
   SetOwner(Self, CurrentTask());

   BYTE reload = FALSE;
   if (!Self->ActivationCount) reload = TRUE;

   LONG i, j;
   if ((Self->ActivationCount) AND (!Self->Procedure) AND (!Self->ProcedureID)) {
      // If no procedure has been specified, kill the old Lua instance to restart from scratch

      FLUID_Free(Self, NULL);

      if (!(prv->Lua = lua_open())) {
         LogErrorMsg("Failed to open a Lua instance.");
         goto failure;
      }

      reload = TRUE;
   }

   if (reload) {
      MSG("The Lua script will be reloaded.");

      lua_gc(prv->Lua, LUA_GCSTOP, 0);  // Stop collector during initialization
         luaL_openlibs(prv->Lua);  // Open Lua libraries
      lua_gc(prv->Lua, LUA_GCRESTART, 0);

      // Register private variables in the registry, which is tamper proof from the user's Lua code

      prv->Lua->Script = Self;

      if (register_interfaces(Self) != ERR_Okay) goto failure;

      // Line hook, executes on the execution of a new line

      if (Self->Flags & SCF_DEBUG) {
         // LUA_MASKLINE:  Interpreter is executing a line.
         // LUA_MASKCALL:  Interpreter is calling a function.
         // LUA_MASKRET:   Interpreter returns from a function.
         // LUA_MASKCOUNT: The hook will be called every X number of instructions executed.

         lua_sethook(prv->Lua, hook_debug, LUA_MASKCALL|LUA_MASKRET|LUA_MASKLINE, 0);
      }

      LONG result;
      if (!StrCompare(LUA_COMPILED, Self->String, 0, 0)) { // The source is compiled
         MSG("Loading pre-compiled Lua script.");
         LONG headerlen = StrLength(Self->String) + 1;
         result = luaL_loadbuffer(prv->Lua, Self->String + headerlen, prv->LoadedSize - headerlen, "DefaultChunk");
      }
      else {
         MSG("Compiling Lua script.");
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
               while ((errorstr[i]) AND (errorstr[i] != ':')) i++;
               if (errorstr[i] IS ':') i++;
               i = StrFormat(buffer, sizeof(buffer), "Line %d: %s\n", line+Self->LineOffset, errorstr + i);
               CSTRING str = Self->String;

               for (j=1; j <= line+1; j++) {
                  if (j >= line-1) {
                     i += StrFormat(buffer+i, sizeof(buffer)-i, "%d: ", j+Self->LineOffset);
                     LONG k;
                     for (k=0; (str[k]) AND (str[k] != '\n') AND (str[k] != '\r') AND (k < 120); k++) {
                        if (i >= sizeof(buffer)-1) break;
                        buffer[i++] = str[k];
                     }
                     if (k IS 120) {
                        for (k=0; (k < 3) AND (i < sizeof(buffer)-1); k++) buffer[i++] = '.';
                     }
                     if (i < sizeof(buffer)-1) buffer[i++] = '\n';
                     buffer[i] = 0;
                  }
                  if (!(str = StrNextLine(str))) break;
               }
               SetString(Self, FID_ErrorString, buffer);

               LogErrorMsg("Parser Failed: %s", Self->ErrorString);
            }
            else LogErrorMsg("Parser Failed: %s", errorstr);
         }

         lua_pop(prv->Lua, 1);  // Pop error string
         goto failure;
      }
      else MSG("Script successfully compiled.");

      if (prv->SaveCompiled) { // Compile the script and save the result to the cache file
         LogMsg("Compiling the source into the cache file.");

         prv->SaveCompiled = FALSE;

         objFile *cachefile;
         if (!CreateObject(ID_FILE, NF_INTEGRAL, &cachefile,
               FID_Path|TSTR,         Self->CacheFile,
               FID_Flags|TLONG,       FL_NEW|FL_WRITE,
               FID_Permissions|TLONG, prv->CachePermissions,
               TAGEND)) {

            save_binary(Self, cachefile->Head.UniqueID);

            SetPointer(cachefile, FID_Date, &prv->CacheDate);
            acFree(cachefile);
         }
      }
   }
   else MSG("Using the Lua script cache.");

   Self->ActivationCount++;

   if ((Self->Procedure) OR (Self->ProcedureID)) {
      // The Lua script needs to have been executed at least once in order for the procedures to be initialised and recognised.

      if ((Self->ActivationCount IS 1) OR (reload)) {
         FMSG("~","Collecting functions prior to procedure call...");
         if (lua_pcall(prv->Lua, 0, 0, 0)) {
            Self->Error = ERR_Failed;
            process_error(Self, "Activation");
         }
         STEP();
      }
   }

   if (!Self->Error) {
      error = run_script(Self);
   }

   error = ERR_Okay; // The error reflects on the initial processing of the script only - the developer must check the Error field for information on script execution

failure:

   if (prv->Lua) {
      FMSG("~","Collecting garbage.");
         lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
      STEP();
   }

   // Change back to the original owner if it still exists.  If it doesn't, self-terminate.

   if (owner_id) {
      OBJECTPTR owner;
      if (!AccessObject(owner_id, 5000, &owner)) {
         SetOwner(Self, owner);
         ReleaseObject(owner);
      }
      else {
         LogMsg("Owner #%d no longer exists - self-terminating.", owner_id);
         acFree(Self);
      }
   }

   Self->ScriptOwnerID = 0;
   prv->Recurse--;
   LogBack();
   return error;
}

//****************************************************************************

static ERROR FLUID_DataFeed(objScript *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->DataType IS DATA_TEXT) {
      SetString(Self, FID_String, Args->Buffer);
   }
   else if (Args->DataType IS DATA_XML) {
      SetString(Self, FID_String, Args->Buffer);
   }
   else if (Args->DataType IS DATA_INPUT_READY) {
      struct prvFluid *prv = Self->Head.ChildPrivate;

      FMSG("~","Incoming input for surface #%d", Args->ObjectID);

      struct InputMsg *input;
      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         UBYTE processed = FALSE;
         for (struct finput *list = prv->InputList; list; list=list->Next) {
            if (((list->SurfaceID IS input->RecipientID) OR (!list->SurfaceID)) AND (list->Mode IS FIM_DEVICE)) {
               processed = TRUE;
               // Execute the callback associated with this input subscription.

               LONG step = GetResource(RES_LOG_DEPTH); // Required as thrown errors cause the debugger to lose its step position

                  lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, list->Callback); // +1 Reference to callback
                  lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, list->InputObject); // +1 Input object
                  named_struct_to_table(prv->Lua, "InputMsg", input); // +1 Input message

                  if (lua_pcall(prv->Lua, 2, 0, 0)) {
                     process_error(Self, "Input DataFeed Callback");
                  }

               SetResource(RES_LOG_DEPTH, step);
            }
         }

         if (!processed) {
            LogF("@","Dangling input feed subscription on surface #%d", input->RecipientID);
            if (gfxUnsubscribeInput(input->RecipientID) IS ERR_NotFound) {
               gfxUnsubscribeInput(0);
            }
         }
      }

      FMSG("~","Collecting garbage.");
        lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
      STEP();

      STEP();
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR FLUID_DerefProcedure(objScript *Self, struct scDerefProcedure *Args)
{
   if (!Args) return ERR_NullArgs;

   if ((Args->Procedure) AND (Args->Procedure->Type IS CALL_SCRIPT)) {
      if (Args->Procedure->Script.Script IS &Self->Head) { // Verification of ownership
         struct prvFluid *prv;
         if (!(prv = Self->Head.ChildPrivate)) return LogCode(ERR_ObjectCorrupt);

         MSG("Dereferencing procedure #" PF64(), Args->Procedure->Script.ProcedureID);

         if (Args->Procedure->Script.ProcedureID) {
            luaL_unref(prv->Lua, LUA_REGISTRYINDEX, Args->Procedure->Script.ProcedureID);
            Args->Procedure->Script.ProcedureID = 0;
         }
         return ERR_Okay;
      }
      else return LogCode(ERR_Args);
   }
   else return LogCode(ERR_Args);
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
   if ((!Args) OR (!Args->Procedure) OR (!Args->Procedure[0])) return LogCode(ERR_NullArgs);

   struct prvFluid *prv;
   if (!(prv = Self->Head.ChildPrivate)) return LogCode(ERR_ObjectCorrupt);

   if ((!prv->Lua) OR (!Self->ActivationCount)) {
      LogErrorMsg("Cannot resolve function '%s'.  Script requires activation.", Args->Procedure);
      return ERR_NotFound;
   }

   lua_getglobal(prv->Lua, Args->Procedure);
   LONG id = luaL_ref(prv->Lua, LUA_REGISTRYINDEX);
   if ((id != LUA_REFNIL) AND (id != LUA_NOREF)) {
      Args->ProcedureID = id;
      return ERR_Okay;
   }
   else {
      LogErrorMsg("Failed to resolve function name '%s' to an ID.", Args->Procedure);
      return ERR_NotFound;
   }
}

//****************************************************************************

static ERROR FLUID_Init(objScript *Self, APTR Void)
{
   if (Self->Path) {
      if (StrCompare("*.fluid|*.fb|*.lua", Self->Path, 0, STR_WILDCARD) != ERR_Okay) {
         MSG("No support for path '%s'", Self->Path);
         return ERR_NoSupport;
      }
   }

   if ((Self->Head.Flags & NF_RECLASSED) AND (!Self->String)) {
      MSG("No support for reclassed Script with no String field value.");
      return ERR_NoSupport;
   }

   STRING str;
   ERROR error;
   BYTE compile = FALSE;
   LONG loaded_size = 0;
   objFile *src_file = NULL;
   if ((!Self->String) AND (Self->Path)) {
      error = CreateObject(ID_FILE, NF_INTEGRAL, &src_file, FID_Path|TSTR, Self->Path, TAGEND);

      LARGE src_ts, src_size;
      if (!error) {
         error = GetFields(src_file, FID_TimeStamp|TLARGE, &src_ts, FID_Size|TLARGE, &src_size, TAGEND);
      }
      else error = ERR_File;

      if (Self->CacheFile) {
         // Compare the cache file date to the original source.  If they match, OR if there was a problem
         // analysing the original location (i.e. the original location does not exist) then the cache file is loaded
         // instead of the original source code.

         objFile *cache_file;
         if (!CreateObject(ID_FILE, NF_INTEGRAL, &cache_file, FID_Path|TSTR, Self->CacheFile, TAGEND)) {
            LARGE cache_ts, cache_size;
            GetFields(cache_file, FID_TimeStamp|TLARGE, &cache_ts, FID_Size|TLARGE, &cache_size, TAGEND);
            acFree(cache_file);

            if ((cache_ts IS src_ts) OR (error)) {
               LogMsg("Using cache '%s'", Self->CacheFile);
               if (!AllocMemory(cache_size, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, &Self->String, NULL)) {
                  LONG len;
                  error = ReadFile(Self->CacheFile, Self->String, cache_size, &len);
                  loaded_size = cache_size;
               }
               else error = ERR_AllocMemory;
            }
         }
      }

      if ((!error) AND (!loaded_size)) {
         if (!AllocMemory(src_size+1, MEM_STRING|MEM_NO_CLEAR, &Self->String, NULL)) {
            LONG len;
            if (!ReadFile(Self->Path, Self->String, src_size, &len)) {
               Self->String[len] = 0;

               // Unicode BOM handler - in case the file starts with a BOM header.
               CSTRING bomptr = check_bom(Self->String);
               if (bomptr != Self->String) CopyMemory(bomptr, Self->String, (len + 1) - (bomptr - Self->String));

               loaded_size = len;

               if (Self->CacheFile) compile = TRUE; // Saving a compilation of the source is desired
            }
            else {
               MSG("Failed to read " PF64() " bytes from '%s'", src_size, Self->Path);
               FreeMemory(Self->String);
               Self->String = NULL;
               error = ERR_ReadFile;
            }
         }
         else error = ERR_AllocMemory;
      }
   }
   else error = ERR_Okay;

   // Allocate private structure

   struct prvFluid *prv;
   if (!error) {
      if (!AllocMemory(sizeof(struct prvFluid), Self->Head.MemFlags, &Self->Head.ChildPrivate, NULL)) {
         prv = Self->Head.ChildPrivate;
         if ((prv->SaveCompiled = compile)) {
            struct DateTime *dt;
            if (!GetPointer(src_file, FID_Date, &dt)) prv->CacheDate = *dt;
            GetLong(src_file, FID_Permissions, &prv->CachePermissions);
            prv->LoadedSize = loaded_size;
         }
      }
      else error = ERR_AllocMemory;
   }

   if (src_file) { acFree(src_file); src_file = NULL; }

   if (error) return LogCode(error);

   //if (!(prv->ObjectList = VarNew(0, 0))) return LogCode(ERR_AllocMemory);

   MSG("Opening a Lua instance.");

   #ifdef DEBUG
      Self->Flags |= SCF_DEBUG;
   #endif

   if (!(prv->Lua = lua_open())) {
      LogErrorMsg("Failed to open a Lua instance.");
      FreeMemory(Self->Head.ChildPrivate);
      Self->Head.ChildPrivate = NULL;
      return ERR_Failed;
   }

   if (!(str = Self->String)) {
      MSG("No statement specified at this stage.");
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
   if ((!Args) OR (!Args->DestID)) return LogCode(ERR_NullArgs);

   if (!Self->String) return LogCode(ERR_FieldNotSet);

   LogBranch("Compiling the statement...");

   struct prvFluid *prv;
   if (!(prv = Self->Head.ChildPrivate)) return LogCode(ERR_ObjectCorrupt);

   if (!luaL_loadstring(prv->Lua, Self->String)) {
      ERROR error = save_binary(Self, Args->DestID);
      LogBack();
      return error;
   }
   else {
      CSTRING str = lua_tostring(prv->Lua,-1);
      lua_pop(prv->Lua, 1);
      LogErrorMsg("Compile Failure: %s", str);
      LogBack();
      return ERR_InvalidData;
   }
}

/*****************************************************************************

-FIELD-
Procedures: Returns a string array of all named procedures defined by a script.

A string array of all procedures loaded into a script is returned by this function. The script will need to have been
activated before reading this field, or an empty list will be returned.

The procedure list is built at the time of the call.  The array is allocated as a memory block and will need to be
removed by the caller with FreeMemory().
-END-

*****************************************************************************/

static ERROR GET_Procedures(objScript *Self, STRING **Value, LONG *Elements)
{
   struct prvFluid *prv;
   if ((prv = Self->Head.ChildPrivate)) {
      LONG memsize = 1024 * 64;
      UBYTE *list;
      if (!AllocMemory(memsize, MEM_DATA|MEM_NO_CLEAR, &list, NULL)) {
         LONG total = 0;
         LONG size  = 0;
         lua_pushnil(prv->Lua);
         while (lua_next(prv->Lua, LUA_GLOBALSINDEX)) {
            if (lua_type(prv->Lua, -1) IS LUA_TFUNCTION) {
               CSTRING name = lua_tostring(prv->Lua, -2);
               size += StrCopy(name, list+size, memsize - size) + 1;
               total++;
            }
            lua_pop(prv->Lua, 1);
         }

         *Value = StrBuildArray(list, size, total, SBF_SORT);
         *Elements = total;

         FreeMemory(list);
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
   struct prvFluid *prv;
   OBJECTPTR dest;
   const Proto *f;
   LONG i;

   LogF("~save_binary()","Save Symbols: %d", Self->Flags & SCF_DEBUG);

   if (!(prv = Self->Head.ChildPrivate)) return LogBackError(0, ERR_ObjectCorrupt);

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
         return LogBackError(0, ERR_Write);
      }
      ReleaseObject(dest);
   }

   if ((code_writer_id > 0) AND ((dest = GetObjectPtr(FileID)))) {
      luaU_dump(prv->Lua, f, &code_writer, dest, (Self->Flags & SCF_DEBUG) ? 0 : 1);
   }
   else luaU_dump(prv->Lua, f, &code_writer_id, (void *)(MAXINT)FileID, (Self->Flags & SCF_DEBUG) ? 0 : 1);

   LogBack();
   return ERR_Okay;
*/
}

//****************************************************************************

static ERROR run_script(objScript *Self)
{
   struct prvFluid *prv = Self->Head.ChildPrivate;

   FMSG("~run_script()","Procedure: %s, Top: %d", Self->Procedure, lua_gettop(prv->Lua));

   prv->CaughtError = ERR_Okay;
   struct object * release_list[8];
   LONG r = 0;
   LONG top;
   LONG pcall_failed = FALSE;
   if ((Self->Procedure) OR (Self->ProcedureID)) {
      if (Self->Procedure) lua_getglobal(prv->Lua, Self->Procedure);
      else lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, Self->ProcedureID);

      if (lua_isfunction(prv->Lua, -1)) {
         if (Self->Flags & SCF_DEBUG) LogF("~","Executing procedure: %s, Args: %d", Self->Procedure, Self->TotalArgs);

         top = lua_gettop(prv->Lua);

         LONG count = 0;
         const struct ScriptArg *args;
         if ((args = Self->ProcArgs)) {
            LONG i;
            for (i=0; i < Self->TotalArgs; i++, args++) {
               LONG type = args->Type;

               if (type & FD_ARRAY) {
                  MSG("Setting arg '%s', Array: %p", args->Name, args->Address);

                  APTR values = args->Address;
                  LONG total_elements = -1;
                  CSTRING arg_name = args->Name;
                  if (args[1].Type & FD_ARRAYSIZE) {
                     if (args[1].Type & FD_LONG) total_elements = args[1].Long;
                     else if (args[1].Type & FD_LARGE) total_elements = args[1].Large;
                     else values = NULL;
                     i++; args++; // Because we took the array-size parameter into account
                  }
                  else MSG("The size of the array is not defined.");

                  if (values) {
                     make_any_table(prv->Lua, type, arg_name, total_elements, values);

                     if (type & FD_ALLOC) FreeMemory(values);
                  }
                  else lua_pushnil(prv->Lua);
               }
               else if (type & FD_STR) {
                  MSG("Setting arg '%s', Value: %.20s", args->Name, (CSTRING)args->Address);
                  lua_pushstring(prv->Lua, args->Address);
               }
               else if (type & FD_STRUCT) { // Pointer to a struct
                  if (args->Address) {
                     if (named_struct_to_table(prv->Lua, args->Name, args->Address) != ERR_Okay) lua_pushnil(prv->Lua);
                     if (type & FD_ALLOC) FreeMemory(args->Address);
                  }
                  else lua_pushnil(prv->Lua);
               }
               else if (type & (FD_PTR|FD_BUFFER)) {
                  // Try and make the pointer safer/more usable by translating it into a buffer, object ID or whatever.
                  // (In a secure environment, pointers may be passed around but may be useless if their use is
                  // disallowed within Lua).

                  MSG("Setting arg '%s', Value: %p", args->Name, args->Address);
                  if ((type & FD_BUFFER) AND (i+1 < Self->TotalArgs) AND (args[1].Type & FD_BUFSIZE)) {
                     // Buffers are considered to be directly writable regions of memory, so the array interface is
                     // used to represent them.
                     if (args[1].Type & FD_LONG) make_array(prv->Lua, FD_BYTE|FD_WRITE, NULL, args->Address, args[1].Long, FALSE);
                     else if (args[1].Type & FD_LARGE) make_array(prv->Lua, FD_BYTE|FD_WRITE, NULL, args->Address, args[1].Large, FALSE);
                     else lua_pushnil(prv->Lua);
                     i++; args++; // Because we took the buffer-size parameter into account
                  }
                  else if (type & FD_OBJECT) {
                     // Pushing direct object pointers is considered safe because they are treated as detached, then
                     // a lock is gained for the duration of the call that is then released on return.  This is a
                     // solid optimisation that also protects the object from unwarranted termination during the call.

                     if (args->Address) {
                        struct object *obj = push_object(prv->Lua, (OBJECTPTR)args->Address);
                        if ((r < ARRAYSIZE(release_list)) AND (access_object(obj))) {
                           release_list[r++] = obj;
                        }
                     }
                     else lua_pushnil(prv->Lua);
                  }
                  else lua_pushlightuserdata(prv->Lua, args->Address);
               }
               else if (type & FD_LONG)   {
                  MSG("Setting arg '%s', Value: %d", args->Name, args->Long);
                  if (type & FD_OBJECT) {
                     if (args->Long) push_object_id(prv->Lua, args->Long);
                     else lua_pushnil(prv->Lua);
                  }
                  else lua_pushinteger(prv->Lua, args->Long);
               }
               else if (type & FD_LARGE)  { MSG("Setting arg '%s', Value: " PF64(), args->Name, args->Large); lua_pushnumber(prv->Lua, args->Large); }
               else if (type & FD_DOUBLE) { MSG("Setting arg '%s', Value: %.2f", args->Name, args->Double); lua_pushnumber(prv->Lua, args->Double); }
               else { lua_pushnil(prv->Lua); LogErrorMsg("Arg '%s' uses unrecognised type $%.8x", args->Name, type); }
               count++;
            }
         }

         LONG step = GetResource(RES_LOG_DEPTH);

         if (lua_pcall(prv->Lua, count, LUA_MULTRET, 0)) {
            pcall_failed = TRUE;
         }

         SetResource(RES_LOG_DEPTH, step);

         while (r > 0) release_object(release_list[--r]);

         if (Self->Flags & SCF_DEBUG) LogBack();
      }
      else {
         char buffer[200];
         StrFormat(buffer, sizeof(buffer), "Procedure '%s' / #" PF64() " does not exist in the script.", Self->Procedure, Self->ProcedureID);
         SetString(Self, FID_ErrorString, buffer);
         LogErrorMsg("%s", buffer);

         #ifdef DEBUG
            STRING *list;
            LONG total_procedures;
            if (!GET_Procedures(Self, &list, &total_procedures)) {
               LONG i;
               for (i=0; i < total_procedures; i++) LogMsg("%s", list[i]);
               FreeMemory(list);
            }
         #endif

         STEP();
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
            MSG("Result: %d/%d: %s", i, results, array[i]);
         }
         array[i] = NULL;
         SetArray(Self, FID_Results, array, i);

         lua_pop(prv->Lua, results);  // pop returned values
      }

      STEP();
      return ERR_Okay;
   }
   else {
      process_error(Self, Self->Procedure ? Self->Procedure : "run_script");
      STEP();
      return Self->Error;
   }
}

//****************************************************************************

static ERROR register_interfaces(objScript *Self)
{
   LogF("~6register_interfaces()","Registering Parasol and Fluid interfaces with Lua.");

   struct prvFluid *prv = Self->Head.ChildPrivate;

   register_array_class(prv->Lua);
   register_object_class(prv->Lua);
   register_module_class(prv->Lua);
   register_struct_class(prv->Lua);
   register_thread_class(prv->Lua);
   register_input_class(prv->Lua);
   register_number_class(prv->Lua);

   lua_register(prv->Lua, "arg", fcmd_arg);
   lua_register(prv->Lua, "catch", fcmd_catch);
   lua_register(prv->Lua, "check", fcmd_check);
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
   lua_register(prv->Lua, "processMessages", fcmd_processMessages);
   lua_register(prv->Lua, "MAKESTRUCT", MAKESTRUCT);

   load_include(Self, "core");

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR create_fluid(void)
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
