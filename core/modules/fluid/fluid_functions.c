
//****************************************************************************

static void clear_subscriptions(objScript *Self)
{
   struct prvFluid *prv;
   if (!(prv = Self->Head.ChildPrivate)) return;

   // Free action subscriptions

   struct actionmonitor *action = prv->ActionList;
   while (action) {
      struct actionmonitor *nextaction = action->Next;

      if (action->ObjectID) {
         OBJECTPTR object;
         if (!AccessObject(action->ObjectID, 3000, &object)) {
            UnsubscribeAction(object, action->ActionID);
            ReleaseObject(object);
         }
      }
      FreeResource(action);
      action = nextaction;
   }
   prv->ActionList = NULL;

   // Free event subscriptions

   struct eventsub *event = prv->EventList;
   while (event) {
      struct eventsub *nextevent = event->Next;
      if (event->EventHandle) UnsubscribeEvent(event->EventHandle);
      FreeResource(event);
      event = nextevent;
   }
   prv->EventList = NULL;

   // Free data requests

   struct datarequest *dr = prv->Requests;
   while (dr) {
      struct datarequest *next = dr->Next;
      FreeResource(dr);
      dr = next;
   }
   prv->Requests = NULL;
}

//****************************************************************************
// check() is the equivalent of an assert() for error codes.  Any error code other than Okay will be converted to an
// exception containing a readable string for the error code.  It is most powerful when used in conjunction with
// the catch() function, which will apply the line number of the exception to the result.  The error code will
// also be propagated to the Script object's Error field.
//
// This function also serves a dual purpose in that it can be used to raise exceptions when an error condition needs to
// be propagated.

static int fcmd_check(lua_State *Lua)
{
   LONG type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      ERROR error = lua_tonumber(Lua, 1);
      if (error) {
         struct prvFluid *prv = Lua->Script->Head.ChildPrivate;
         prv->CaughtError = error;
         luaL_error(prv->Lua, GetErrorMsg(error));
      }
   }
   return 0;
}

//****************************************************************************
// Use catch() to switch on exception handling for functions that return an error code other than ERR_Okay.  Areas
// affected include obj.new(); any module function that returns an ERROR; any method or action called on an object.
// The caught error code is returned by default, or if no exception handler is defined then the entire exception table
// is returned.
//
//   err = catch(function()
//      // Code to execute
//   end,
//   function(Exception)
//      // Exception handler
//      print("Code: " .. nz(Exception.code,"LUA") .. ", Message: " .. Exception.message)
//   end)
//
// As above, but the handler is only called if certain codes are raised.  Any mismatched errors will throw to the parent code.
//
//   err = catch(function()
//      // Code to execute
//   end,
//   { ERR_Failed, ERR_Terminate }, // Errors to filter for
//   fuction(Exception) // Exception handler for the filtered errors
//   end)
//
// To silently ignore exceptions, or to receive the thrown exception details as a table result:
//
//   local exception = catch(function()
//      // Code to execute
//   end)

static int fcmd_catch_handler(lua_State *Lua)
{
   lua_Debug ar;
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;
   if (lua_getstack(Lua, 2, &ar)) {
      lua_getinfo(Lua, "nSl", &ar);
      // ar.currentline, ar.name, ar.source, ar.short_src, ar.linedefined, ar.lastlinedefined, ar.what
      prv->ErrorLine = ar.currentline;
   }
   else prv->ErrorLine = -1;

   return 1; // Return 1 to rethrow the exception table, no need to re-push the value
}

static int fcmd_catch(lua_State *Lua)
{
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   if (lua_gettop(Lua) >= 2) {
      LONG type = lua_type(Lua, 1);
      if (type IS LUA_TFUNCTION) {
         LONG catch_filter = 0;
         type = lua_type(Lua, 2);

         LONG a = 2;
         if (type IS LUA_TTABLE) {
            // First argument is a list of error codes to filter on, second argument is the exception handler.
            lua_pushvalue(Lua, a++);
            catch_filter = luaL_ref(Lua, LUA_REGISTRYINDEX);
            type = lua_type(Lua, a);
         }

         if (type IS LUA_TFUNCTION) {
            BYTE caught_by_filter = FALSE;
            prv->Catch++; // Convert ERROR results to exceptions.
            prv->CaughtError = ERR_Okay;
            lua_pushcfunction(Lua, fcmd_catch_handler);
            lua_pushvalue(Lua, 1); // Parameter #1 is the function to call.
            LONG result_top = lua_gettop(Lua);
            if (lua_pcall(Lua, 0, LUA_MULTRET, -2)) { // An exception was raised!
               prv->Catch--;

               if ((prv->CaughtError != ERR_Okay) AND (catch_filter)) { // Apply error code filtering
                  lua_rawgeti(Lua, LUA_REGISTRYINDEX, catch_filter);
                  lua_pushnil(Lua);  // First key
                  while ((!caught_by_filter) AND (lua_next(Lua, -2) != 0)) { // Iterate over each table key
                     // -1 is the value and -2 is the key.
                     if (lua_tointeger(Lua, -1) IS prv->CaughtError) {
                        caught_by_filter = TRUE;
                        lua_pop(Lua, 1); // Pop the key because we're going to break the loop early.
                     }
                     lua_pop(Lua, 1); // Removes 'value'; keeps 'key' for next iteration
                  }
                  lua_pop(Lua, 1); // Pop the catch_filter
               }
               else caught_by_filter = TRUE;

               if (catch_filter) luaL_unref(Lua, LUA_REGISTRYINDEX, catch_filter);

               if (caught_by_filter) {
                  lua_pushvalue(Lua, a); // For lua_call()

                  // Build an exception table: { code=123, message="Description" }

                  lua_newtable(Lua);
                  lua_pushstring(Lua, "code");
                  if (prv->CaughtError != ERR_Okay) lua_pushinteger(Lua, prv->CaughtError);
                  else lua_pushnil(Lua);
                  lua_settable(Lua, -3);

                  lua_pushstring(Lua, "message");
                  lua_pushvalue(Lua, -4); // This is the error exception string returned by pcall()
                  lua_settable(Lua, -3);

                  lua_pushstring(Lua, "line");
                  lua_pushinteger(Lua, prv->ErrorLine);
                  lua_settable(Lua, -3);

                  lua_call(Lua, 1, 0); // nargs, nresults

                  lua_pop(Lua, 1); // Pop the error message.
               }
               else luaL_error(Lua, lua_tostring(Lua, -1)); // Rethrow the message

               lua_pushinteger(Lua, prv->CaughtError ? prv->CaughtError : ERR_Exception);
               return 1;
            }
            else { // pcall() was successful
               prv->Catch--;
               if (catch_filter) luaL_unref(Lua, LUA_REGISTRYINDEX, catch_filter);
               lua_pushinteger(Lua, ERR_Okay);
               LONG result_count = lua_gettop(Lua) - result_top + 1;
               lua_insert(Lua, -result_count); // Push the error code in front of any other results
               return result_count;
            }
         }
         else {
            if (catch_filter) luaL_unref(Lua, LUA_REGISTRYINDEX, catch_filter);
            luaL_argerror(Lua, 2, "Expected function.");
         }
      }
      else luaL_argerror(Lua, 1, "Expected function.");
   }
   else { // In single-function mode, exceptions are returned as a result.
      LONG type = lua_type(Lua, 1);
      if (type IS LUA_TFUNCTION) {
         prv->Catch++; // Indicate to other routines that errors must be converted to exceptions.
         prv->CaughtError = ERR_Okay;

         lua_pushcfunction(Lua, fcmd_catch_handler);
         lua_pushvalue(Lua, 1); // Parameter #1 is the function to call.
         LONG result_top = lua_gettop(Lua);
         if (lua_pcall(Lua, 0, LUA_MULTRET, -2)) {
            prv->Catch--;

            // -1 is the pcall() error string result
            // -2 is fcmd_catch_handler()
            // -3 is the function we called.

            lua_remove(Lua, -2); // Pop the handler
            lua_remove(Lua, -2); // Pop the function

            // Return an exception table: { code=123, message="Description", line=123 }

            lua_newtable(Lua); // +1 stack
            lua_pushstring(Lua, "code");
            if (prv->CaughtError != ERR_Okay) lua_pushinteger(Lua, prv->CaughtError);
            else lua_pushnil(Lua); // Distinguish Lua exceptions by setting the code to nil.
            lua_settable(Lua, -3);

            lua_pushstring(Lua, "message");
            lua_pushvalue(Lua, -3); // Temp duplicate of the reference to -3; the error message returned by pcall()
            lua_settable(Lua, -3);

            lua_pushstring(Lua, "line");
            lua_pushinteger(Lua, prv->ErrorLine);
            lua_settable(Lua, -3);

            lua_remove(Lua, -2); // Remove the error msg to balance the stack
            return 1;
         }
         else {
            prv->Catch--; // Successful call
            lua_pushnil(Lua); // Use nil to indicate that no exception occurred
            LONG result_count = lua_gettop(Lua) - result_top + 1;
            lua_insert(Lua, -result_count); // Push the error code in front of any other results
            return result_count;
         }
      }
      else luaL_argerror(Lua, 1, "Expected function.");
   }

   return 0;
}

//****************************************************************************
// Usage: processMessages(Timeout)
//
// Processes incoming messages.  Returns the number of microseconds that elapsed, followed by the error from
// ProcessMessages().  To process messages until a QUIT message is received, call processMessages(-1)

static int fcmd_processMessages(lua_State *Lua)
{
   static volatile BYTE recursion = 0; // Intentionally accessible to all threads

   FMSG("~","Collecting garbage.");
      lua_gc(Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
   STEP();

   if (recursion) return 0;

   LONG timeout = lua_tointeger(Lua, 1);

   recursion++;

      LARGE time = PreciseTime();
      ERROR error = ProcessMessages(0, timeout);
      time = PreciseTime() - time;

   recursion--;

   lua_pushnumber(Lua, time);
   lua_pushinteger(Lua, error);
   return 2;
}

//****************************************************************************
// The event callback will be called with the following synopsis:
//
// function callback(EventID, Args)
//
// Where Args is a named array containing the event parameters.  If the event is not known to Fluid, then no Args will
// be provided.

static void receive_event(struct eventsub *Event, APTR Info, LONG InfoSize)
{
   FMSG("Fluid","Received event $%.8x%.8x", (LONG)((Event->EventID>>32) & 0xffffffff), (LONG)(Event->EventID & 0xffffffff));

   struct prvFluid *prv;
   objScript *Script = (objScript *)CurrentContext();
   if (!(prv = Script->Head.ChildPrivate)) return;

   lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, Event->Function);

   lua_pushnumber(prv->Lua, ((struct rkEvent *)Info)->EventID);
   if (lua_pcall(prv->Lua, 1, 0, 0)) {
      process_error(Script, "Event Subscription");
   }

   FMSG("~","Collecting garbage.");
      lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
   STEP();
}

//****************************************************************************
// Usage: unsubscribeEvent(handle)

static int fcmd_unsubscribe_event(lua_State *Lua)
{
   struct prvFluid *prv;
   if (!(prv = Lua->Script->Head.ChildPrivate)) return 0;

   APTR handle;
   if ((handle = lua_touserdata(Lua, 1))) {
      if (Lua->Script->Flags & SCF_DEBUG) LogF("unsubscribeevent()","Handle: %p", handle);

      struct eventsub *event;
      for (event=prv->EventList; event; event=event->Next) {
         if (event->EventHandle IS handle) {
            UnsubscribeEvent(event->EventHandle);
            luaL_unref(prv->Lua, LUA_REGISTRYINDEX, event->Function);

            if (event->Prev) event->Prev->Next = event->Next;
            if (event->Next) event->Next->Prev = event->Prev;
            if (event IS prv->EventList) prv->EventList = event->Next;

            FreeResource(event);

            return 0;
         }
      }

      LogF("@unsubscribeevent","Failed to link an event to handle %p.", handle);
   }
   else luaL_argerror(Lua, 1, "No handle provided.");

   return 0;
}

//****************************************************************************
// Usage: error, handle = subscribeEvent("group.subgroup.name", function)

static int fcmd_subscribe_event(lua_State *Lua)
{
   CSTRING event;
   if (!(event = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Event string expected.");
      return 0;
   }

   if (!lua_isfunction(Lua, 2)) {
      if (!lua_isnil(Lua, 2)) {
         luaL_argerror(Lua, 2, "Function or nil expected.");
         return 0;
      }
   }

   // Generate the event ID

   char group[60];
   ULONG group_hash = 0, subgroup_hash = 0;
   LONG i;
   for (i=0; event[i]; i++) {
      if (event[i] IS '.') {
         LONG j;
         if (i >= sizeof(group)) luaL_error(Lua, "Buffer overflow.");
         for (j=0; (j < i) AND (j < sizeof(group)-1); j++) group[j] = event[j];
         group[j] = 0;
         group_hash = StrHash(group, 0);
         event += i + 1;

         for (i=0; event[i]; i++) {
            if (event[i] IS '.') {
               LONG j;
               if (i >= sizeof(group)) luaL_error(Lua, "Buffer overflow.");
               for (j=0; (j < i) AND (j < sizeof(group)-1); j++) group[j] = event[j];
               group[j] = 0;
               subgroup_hash = StrHash(group, 0);
               event += i + 1;
               break;
            }
         }
         break;
      }
   }

   LONG group_id = 0;
   if ((group_hash) AND (subgroup_hash)) {
      switch (group_hash) {
         case HASH_FILESYSTEM: group_id = EVG_FILESYSTEM; break;
         case HASH_NETWORK:    group_id = EVG_NETWORK; break;
         case HASH_USER:       group_id = EVG_USER; break;
         case HASH_SYSTEM:     group_id = EVG_SYSTEM; break;
         case HASH_GUI:        group_id = EVG_GUI; break;
         case HASH_DISPLAY:    group_id = EVG_DISPLAY; break;
         case HASH_IO:         group_id = EVG_IO; break;
         case HASH_HARDWARE:   group_id = EVG_HARDWARE; break;
         case HASH_AUDIO:      group_id = EVG_AUDIO; break;
         case HASH_POWER:      group_id = EVG_POWER; break;
         case HASH_CLASS:      group_id = EVG_CLASS; break;
         case HASH_APP:        group_id = EVG_APP; break;
      }
   }

   if (!group_id) {
      luaL_error(Lua, "Invalid group name '%s' in event string.", event);
      return 0;
   }

   EVENTID event_id = GetEventID(group_id, group, event);

   struct eventsub *eventsub;
   ERROR error;
   if (!event_id) {
      luaL_argerror(Lua, 1, "Failed to build event ID.");
      lua_pushinteger(Lua, ERR_Failed);
      return 1;
   }
   else if (!(error = AllocMemory(sizeof(struct eventsub), MEM_DATA, &eventsub, NULL))) {
      FUNCTION call;

      SET_FUNCTION_STDC(call, receive_event);
      if (!(error = SubscribeEvent(event_id, &call, eventsub, &eventsub->EventHandle))) {
         struct prvFluid *prv = Lua->Script->Head.ChildPrivate;
         lua_settop(Lua, 2);
         eventsub->Function = luaL_ref(Lua, LUA_REGISTRYINDEX);
         eventsub->EventID  = event_id;
         eventsub->Next     = prv->EventList;
         if (prv->EventList) prv->EventList->Prev = eventsub;
         prv->EventList = eventsub;

         lua_pushlightuserdata(Lua, eventsub->EventHandle); // 1: Handle
         lua_pushinteger(Lua, error); // 2: Error code
         return 2;
      }
      else FreeResource(eventsub);
   }

   lua_pushnil(Lua); // Handle
   lua_pushinteger(Lua, error); // Error code
   return 2;
}

//****************************************************************************
// Usage: msg("Message")
// Prints a debug message, with no support for input parameters.  This is the safest way to call LogF().

static int fcmd_msg(lua_State *Lua)
{
   int n = lua_gettop(Lua);  // number of arguments
   int i;

   lua_getglobal(Lua, "tostring");
   for (i=1; i <= n; i++) {
      lua_pushvalue(Lua, -1);  // function to be called (tostring)
      lua_pushvalue(Lua, i);   // value to pass to tostring
      lua_call(Lua, 1, 1);
      CSTRING s = lua_tostring(Lua, -1);  // get result
      if (!s) return luaL_error(Lua, LUA_QL("tostring") " must return a string to " LUA_QL("print"));

      LogF("Fluid","%s", s);

      lua_pop(Lua, 1);  // pop the string result
   }
   return 0;
}

//****************************************************************************
// Usage: print(...)
// Prints a message to stderr.  On Android stderr is unavailable, so the message is printed in the debug output.

static int fcmd_print(lua_State *Lua)
{
   int n = lua_gettop(Lua);  // number of arguments
   int i;

   lua_getglobal(Lua, "tostring");
   for (i=1; i <= n; i++) {
      lua_pushvalue(Lua, -1);  // function to be called
      lua_pushvalue(Lua, i);   // value to print
      lua_call(Lua, 1, 1);
      const char *s = lua_tostring(Lua, -1);  // get result
      if (!s) {
         return luaL_error(Lua, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
      }
      #ifdef __ANDROID__
         LogF("Fluid","%s", s);
      #else
         fprintf(stderr, "%s", s);
      #endif
      lua_pop(Lua, 1);  // pop result
   }
   fprintf(stderr, "\n");
   return 0;
}

/*****************************************************************************
** Usage: include "File1","File2","File3",...
*/

static int fcmd_include(lua_State *Lua)
{
   if (!lua_isstring(Lua, 1)) {
      luaL_argerror(Lua, 1, "Include name(s) required.");
      return 0;
   }

   LONG top = lua_gettop(Lua);
   LONG n;
   for (n=1; n <= top; n++) {
      CSTRING include = lua_tostring(Lua, n);

      ERROR error;
      if ((error = load_include(Lua->Script, include))) {
         if (error IS ERR_FileNotFound) luaL_error(Lua, "Requested include file '%s' does not exist.", include);
         else luaL_error(Lua, "Failed to process include file: %s", GetErrorMsg(error));
         return 0;
      }
   }

   return 0;
}

/*****************************************************************************
** Usage: require "Module"
**
** Loads a Fluid language file from system:scripts/ and executes it.  Differs from loadFile() in that registration
** prevents multiple executions and the folder restriction improves security.
*/

static int fcmd_require(lua_State *Lua)
{
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   CSTRING module, error_msg = NULL;
   ERROR error = ERR_Okay;
   if ((module = lua_tostring(Lua, 1))) {
      // For security purposes, check the validity of the module name.

      LONG i;
      LONG slash_count = 0;
      for (i=0; module[i]; i++) {
         if ((module[i] >= 'a') AND (module[i] <= 'z')) continue;
         if ((module[i] >= 'A') AND (module[i] <= 'Z')) continue;
         if ((module[i] >= '0') AND (module[i] <= '9')) continue;
         if (module[i] IS '/') { slash_count++; continue; }
         break;
      }

      if ((module[i]) OR (i >= 32) OR (slash_count > 1)) {
         luaL_error(Lua, "Invalid module name; only alpha-numeric names are permitted with max 32 chars.");
         return 0;
      }

      // Check if the module is already loaded.

      char req[40] = "require.";
      StrCopy(module, req+8, sizeof(req)-8);

      lua_getfield(prv->Lua, LUA_REGISTRYINDEX, req);
      BYTE loaded = lua_toboolean(prv->Lua, -1);
      lua_pop(prv->Lua, 1);
      if (loaded) return 0;

      char path[96];
      StrFormat(path, sizeof(path), "system:scripts/%s.fluid", module);

      objFile *file;
      if (!(error = CreateObject(ID_FILE, 0, &file,
            FID_Path|TSTR,   path,
            FID_Flags|TLONG, FL_READ,
            TAGEND))) {

         APTR buffer;
         if (!AllocMemory(SIZE_READ, MEM_NO_CLEAR, &buffer, NULL)) {
            struct code_reader_handle handle = { file, buffer };
            if (!lua_load(Lua, &code_reader, &handle, module)) {
               prv->RequireCounter++; // Used by getExecutionState()
               if (!lua_pcall(Lua, 0, 0, 0)) {
                  // Success, mark the module as loaded.

                  lua_pushboolean(prv->Lua, 1);
                  lua_setfield(prv->Lua, LUA_REGISTRYINDEX, req);
               }
               else error_msg = lua_tostring(Lua, -1);
               prv->RequireCounter--;
            }
            else error_msg = lua_tostring(Lua, -1);
            FreeResource(buffer);
         }
         else error = ERR_AllocMemory;
         acFree(file);
      }
      else error = ERR_File;
   }
   else luaL_argerror(Lua, 1, "Expected module name.");

   if (error_msg) luaL_error(Lua, error_msg);
   else if (error) luaL_error(Lua, GetErrorMsg(error));

   return 0;
}

/*****************************************************************************
** Usage: state = getExecutionState()
**
** Returns miscellaneous information about the code's current state of execution.
*/

static int fcmd_get_execution_state(lua_State *Lua)
{
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   lua_newtable(Lua);

   lua_pushstring(Lua, "inRequire");
   lua_pushboolean(Lua, prv->RequireCounter ? TRUE : FALSE);
   lua_settable(Lua, -3);

   return 1;
}

/*****************************************************************************
** Usage: loadFile("Path")
**
** Loads a Fluid language file from any location and executes it.
*/

static int fcmd_loadfile(lua_State *Lua)
{
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   CSTRING error_msg = NULL;
   LONG results = 0;
   ERROR error = ERR_Okay;

   CSTRING path, name;
   if ((path = lua_tostring(Lua, 1))) {
      UBYTE fbpath[StrLength(path)+6];

      LogF("loadfile()","%s", path);

      BYTE recompile = FALSE;
      CSTRING src = path;

      LONG pathlen = StrLength(path);

      if (!StrMatch(".fluid", path + pathlen - 6)) {
         // File is a .fluid.  Let's check if a .fb exists and is date-stamped for the same date as the .fluid version.
         // Note: The developer can also delete the .fluid file in favour of a .fb that is already present (for
         // production releases)

         StrCopy(path, fbpath, pathlen - 5);
         StrCopy(".fb", fbpath + pathlen - 6, COPY_ALL);

         LogF("loadfile","Checking for a compiled Fluid file: %s", fbpath);

         objFile *fb_file, *src_file;
         if (!CreateObject(ID_FILE, NF_INTEGRAL, &fb_file,
               FID_Path|TSTR, fbpath,
               TAGEND)) {
            // A compiled version exists.  Compare datestamps

            if (!(error = CreateObject(ID_FILE, NF_INTEGRAL, &src_file,
                  FID_Path|TSTR, path,
                  TAGEND))) {
               LARGE fb_ts, src_ts;
               GetLarge(fb_file, FID_TimeStamp, &fb_ts);
               GetLarge(src_file, FID_TimeStamp, &src_ts);

               if (fb_ts != src_ts) {
                  LogMsg("Timestamp mismatch, will recompile the cached version.");
                  recompile = TRUE;
                  error = ERR_Failed;
               }
               else src = fbpath;

               acFree(src_file);
            }
            else if (error IS ERR_FileNotFound) {
               src = fbpath; // Use the .fb if the developer removed the .fluid (typically done for production releases)
            }

            acFree(fb_file);
         }
      }

      objFile *file;
      if (!(error = CreateObject(ID_FILE, 0, &file,
            FID_Path|TSTR,   src,
            FID_Flags|TLONG, FL_READ,
            TAGEND))) {

         APTR buffer;
         if (!AllocMemory(SIZE_READ, MEM_NO_CLEAR, &buffer, NULL)) {
            struct code_reader_handle handle = { file, buffer };

            // Check for the presence of a compiled header and skip it if present **

            LONG len, i;
            UBYTE header[256];

            if (!acRead(file, header, sizeof(header), &len)) {
               if (!StrCompare(LUA_COMPILED, header, 0, 0)) {
                  recompile = FALSE; // Do not recompile that which is already compiled
                  for (i=sizeof(LUA_COMPILED)-1; (i < len) AND (header[i]); i++);
                  if (!header[i]) {
                     i++;
                  }
                  else i = 0;
               }
               else i = 0;
            }
            else i = 0;

            SetLong(file, FID_Position, i);

            // Get the file name from the path

            name = path;
            for (i=0; name[i]; i++);
            while (i > 0) {
               if ((name[i-1] IS '\\') OR (name[i-1] IS '/') OR (name[i-1] IS ':')) break;
               i--;
            }

            if (!lua_load(Lua, &code_reader, &handle, name+i)) {
#warning Code compilation not currently supported
            /*
               if (recompile) {
                  OBJECTPTR cachefile;
                  if (!CreateObject(ID_FILE, 0, &cachefile,
                        FID_Path|TSTR,         fbpath,
                        FID_Flags|TLONG,       FL_NEW|FL_WRITE,
                        FID_Permissions|TLONG, PERMIT_USER_READ|PERMIT_USER_WRITE,
                        TAGEND)) {
                     const Proto *f;
                     struct DateTime *date;
                     f = clvalue(prv->Lua->top + (-1))->l.p;
                     luaU_dump(prv->Lua, f, &code_writer, cachefile, (Self->Flags & SCF_DEBUG) ? 0 : 1);
                     if (!GetPointer(file, FID_Date, &date)) {
                        SetPointer(cachefile, FID_Date, date);
                     }
                     acFree(cachefile);
                  }
               }
             */

               LONG result_top = lua_gettop(prv->Lua);

               if (!lua_pcall(Lua, 0, LUA_MULTRET, 0)) {
                  results = lua_gettop(prv->Lua) - result_top + 1;
               }
               else error_msg = lua_tostring(prv->Lua, -1);
            }
            else error_msg = lua_tostring(prv->Lua, -1);

            FreeResource(buffer);
         }
         else error = ERR_AllocMemory;

         acFree(file);
      }
      else error = ERR_DoesNotExist;
   }
   else {
      luaL_argerror(Lua, 1, "File path required.");
   }

   if ((!error_msg) AND (error)) error_msg = GetErrorMsg(error);
   if (error_msg) luaL_error(Lua, "Failed to load/parse file '%s', error: %s", path, error_msg);

   return results;
}

//****************************************************************************
// Usage: exec(Statement)

struct luaReader {
   CSTRING String;
   LONG Index;
   LONG Size;
};

static const char * code_reader_buffer(lua_State *, void *, size_t *);

static int fcmd_exec(lua_State *Lua)
{
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   LONG results = 0;

   CSTRING statement;
   if ((statement = lua_tostring(Lua, 1))) {
      LogF("~exec()","");

         // Check for the presence of a compiled header and skip it if present

         if (!StrCompare(LUA_COMPILED, statement, 0, 0)) {
            LONG i;
            for (i=sizeof(LUA_COMPILED)-1; statement[i]; i++);
            if (!statement[i]) statement += i + 1;
         }

         struct luaReader lr = { statement, 0, StrLength(statement) };
         CSTRING error_msg;
         if (!lua_load(Lua, &code_reader_buffer, &lr, "exec")) {
            LONG result_top = lua_gettop(prv->Lua);
            if (!lua_pcall(Lua, 0, LUA_MULTRET, 0)) {
               results = lua_gettop(prv->Lua) - result_top + 1;
               error_msg = NULL;
            }
            else error_msg = lua_tostring(prv->Lua, -1);
         }
         else error_msg = lua_tostring(prv->Lua, -1);

      LogBack();
      if (error_msg) luaL_error(Lua, error_msg);
   }
   else luaL_argerror(Lua, 1, "Fluid statement required.");

   return results;
}

//****************************************************************************

static const char * code_reader_buffer(lua_State *Lua, void *Source, size_t *ResultSize)
{
   struct luaReader *lr = (struct luaReader *)Source;
   *ResultSize = lr->Size - lr->Index;
   lr->Index += *ResultSize;
   return lr->String;
}

//****************************************************************************
// Usage: arg = arg("Width", IfNullValue)

static int fcmd_arg(lua_State *Lua)
{
   objScript *Self = Lua->Script;

   LONG args = lua_gettop(Lua);
   CSTRING str;
   if ((str = VarGetString(Self->Vars, lua_tostring(Lua, 1)))) {
      if ((str) AND (str[0])) {
         lua_pushstring(Lua, str);
         return 1;
      }
   }

   if (args IS 2) {
      return 1; // Return value 2 (top of the stack)
   }
   else {
      lua_pushnil(Lua);
      return 1;
   }
}

/*****************************************************************************
** Returns the 2nd argument if the 1st argument is evaluated as nil, zero, an empty string, table or array.  Otherwise
** the 1st argument is returned.
**
** If the 2nd argument is not given, nil is returned if the 1st argument is evaluated as being empty, otherwise 1 is
** returned.
**
** Usage: result = nz(checkval, zeroval)
**
** 'nz' is short for 'nonzero' and its use can be described as 'if checkval is non zero then return checkval, else
** return zeroval'.
*/

static int fcmd_nz(lua_State *Lua)
{
   LONG args = lua_gettop(Lua);
   if ((args != 2) AND (args != 1)) {
      luaL_error(Lua, "Expected 1 or 2 arguments, not %d.", args);
      return 0;
   }

   BYTE isnull = FALSE;
   LONG type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      if (lua_tonumber(Lua, 1)) {
         isnull = FALSE;
      }
      else isnull = TRUE;
   }
   else if (type IS LUA_TSTRING) {
      CSTRING str;
      if ((str = lua_tostring(Lua, 1))) {
         if (str[0]) isnull = FALSE;
         else isnull = TRUE;
      }
      else isnull = TRUE;
   }
   else if ((type IS LUA_TNIL) OR (type IS LUA_TNONE)) {
      isnull = TRUE;
   }
   else if ((type IS LUA_TLIGHTUSERDATA) OR (type IS LUA_TUSERDATA)) {
      if (lua_touserdata(Lua, 1)) isnull = FALSE;
      else isnull = TRUE;
   }
   else if (type IS LUA_TTABLE) {
      if (lua_objlen(Lua, 1) == 0) {
         lua_pushnil(Lua);
         if (lua_next(Lua, 1)) {
            isnull = FALSE;
            lua_pop(Lua, 2); // Remove discovered value and next key
         }
         else isnull = TRUE;
      }
      else isnull = FALSE;
   }

   if (args IS 2) {
      if (isnull) {
         // Return value 2 (top of the stack)
      }
      else {
         // Return value 1
         lua_pop(Lua, 1);
      }
   }
   else {
      if (isnull) return 0;
      else lua_pushinteger(Lua, 1);
   }

   return 1;
}
