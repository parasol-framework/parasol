/*****************************************************************************

The input interface provides support for processing input messages.  The InputMsg structure is passed for each incoming
message that is detected.

   local in = input.subscribe(JTYPE_MOVEMENT, SurfaceID, 0, function(SurfaceID, Message)

   end)

   in.unsubscribe()

To get keyboard feedback:

   local in = input.keyboard(SurfaceID, function(Input, SurfaceID, Flags, Value)

   end)

   in.unsubscribe()

*****************************************************************************/

static int input_unsubscribe(lua_State *Lua);

//****************************************************************************
// Any Read accesses to the object will pass through here.

static int input_index(lua_State *Lua)
{
   struct finput *input;

   if ((input = (struct finput *)luaL_checkudata(Lua, 1, "Fluid.input"))) {
      CSTRING field;
      if (!(field = luaL_checkstring(Lua, 2))) return 0;

      MSG("input.index(#%d, %s)", input->SurfaceID, field);

      switch (StrHash(field, 0)) {
         case HASH_UNSUBSCRIBE:
            lua_pushvalue(Lua, 1); // Duplicate the interface reference
            lua_pushcclosure(Lua, input_unsubscribe, 1);
            return 1;

         default:
            luaL_error(Lua, "Unknown field reference '%s'", field);
      }
   }
   return 0;
}

//****************************************************************************
// Usage: input = input.keyboard(SurfaceID, Function)

static int input_keyboard(lua_State *Lua)
{
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   OBJECTID object_id;
   struct object *object;
   if ((object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) object_id = object->ObjectID;
   else object_id = lua_tointeger(Lua, 1);

   if ((object_id) AND (GetClassID(object_id) != ID_SURFACE)) luaL_argerror(Lua, 1, "Surface object required.");

   LONG function_type = lua_type(Lua, 2);
   if ((function_type IS LUA_TFUNCTION) OR (function_type IS LUA_TSTRING));
   else {
      luaL_argerror(Lua, 2, "Function reference required.");
      return 0;
   }

   FMSG("~input.keyboard()","Surface: %d", object_id);

   BYTE sub_keyevent = FALSE;
   if (object_id) {
      if (!prv->FocusEventHandle) { // Monitor the focus state of the target surface with a global function.
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &focus_event);
         SubscribeEvent(EVID_GUI_SURFACE_FOCUS, &callback, Lua, &prv->FocusEventHandle);
      }

      objSurface *surface;
      if (!AccessObject(object_id, 5000, &surface)) {
         if (surface->Flags & RNF_HAS_FOCUS) sub_keyevent = TRUE;
         ReleaseObject(surface);
      }
      else {
         STEP();
         luaL_error(Lua, "Failed to access surface #%d.", object_id);
         return 0;
      }
   }
   else sub_keyevent = TRUE; // Global subscription independent of any surface.

   struct finput *input;
   if ((input = (struct finput *)lua_newuserdata(Lua, sizeof(struct finput)))) {
      luaL_getmetatable(Lua, "Fluid.input");
      lua_setmetatable(Lua, -2);

      APTR event = NULL;
      if (sub_keyevent) {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, input, &event);
      }

      input->Script    = Lua->Script;
      input->SurfaceID = object_id;
      input->KeyEvent  = event;

      if (function_type IS LUA_TFUNCTION) {
         lua_pushvalue(Lua, 2);
         input->Callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
      }
      else {
         lua_getglobal(Lua, (STRING)lua_tostring(Lua, 2));
         input->Callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
      }

      lua_pushvalue(Lua, lua_gettop(Lua)); // Take a copy of the Fluid.input object
      input->InputObject = luaL_ref(Lua, LUA_REGISTRYINDEX);

      input->Mode = FIM_KEYBOARD;
      input->Next = prv->InputList;
      prv->InputList = input;
      STEP();
      return 1;
   }
   else {
      STEP();
      luaL_error(Lua, "Failed to create Fluid.input object.");
      return 0;
   }

   STEP();
   return 0;
}

//****************************************************************************
// Usage: input = input.subscribe(MaskFlags (JTYPE), SurfaceID (Optional), DeviceID (Optional), Function)
//
// This functionality is a wrapper for the gfxSubscribeInput() function.

static int input_subscribe(lua_State *Lua)
{
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   LONG mask = lua_tointeger(Lua, 1); // Optional

   OBJECTID object_id;
   struct object *object;
   if ((object = get_meta(Lua, 2, "Fluid.obj"))) object_id = object->ObjectID;
   else object_id = lua_tointeger(Lua, 2);

   LONG device_id = lua_tointeger(Lua, 3); // Optional

   LONG function_type = lua_type(Lua, 4);
   if ((function_type IS LUA_TFUNCTION) OR (function_type IS LUA_TSTRING));
   else {
      luaL_argerror(Lua, 4, "Function reference required.");
      return 0;
   }

   ERROR error;
   if (!modDisplay) {
      OBJECTPTR context = SetContext(modFluid);
         error = LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase);
      SetContext(context);

      if (error) {
         luaL_error(Lua, "Failed to load display module.");
         return 0;
      }
   }

   FMSG("input.subscribe()","Surface: %d, Mask: $%.8x, Device: %d", object_id, mask, device_id);

   if (!(error = gfxSubscribeInput(object_id, mask, device_id))) {
      struct finput *input;
      if ((input = (struct finput *)lua_newuserdata(Lua, sizeof(struct finput)))) {
         luaL_getmetatable(Lua, "Fluid.input");
         lua_setmetatable(Lua, -2);

         input->SurfaceID = object_id;

         if (function_type IS LUA_TFUNCTION) {
            lua_pushvalue(Lua, 4);
            input->Callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
         }
         else {
            lua_getglobal(Lua, (STRING)lua_tostring(Lua, 1));
            input->Callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
         }

         lua_pushvalue(Lua, lua_gettop(Lua)); // Take a copy of the Fluid.input object
         input->InputObject = luaL_ref(Lua, LUA_REGISTRYINDEX);
         input->KeyEvent = NULL;
         input->Mode = FIM_DEVICE;
         input->Next = prv->InputList;
         prv->InputList = input;
         return 1;
      }
      else gfxUnsubscribeInput(object_id);
   }

   luaL_error(Lua, "Failed to initialise input subscription.");
   return 0;
}

//****************************************************************************
// Usage: error = input.unsubscribe(SurfaceID)

static int input_unsubscribe(lua_State *Lua)
{
   struct finput *input;
   if (!(input = get_meta(Lua, lua_upvalueindex(1), "Fluid.input"))) {
      luaL_argerror(Lua, 1, "Expected input interface.");
      return 0;
   }

   FMSG("~input.unsubscribe()","");

   if (input->SurfaceID)   { gfxUnsubscribeInput(input->SurfaceID); input->SurfaceID = 0; }
   if (input->InputObject) { luaL_unref(Lua, LUA_REGISTRYINDEX, input->InputObject); input->InputObject = 0; }
   if (input->Callback)    { luaL_unref(Lua, LUA_REGISTRYINDEX, input->Callback); input->Callback = 0; }
   if (input->KeyEvent)    { UnsubscribeEvent(input->KeyEvent); input->KeyEvent = NULL; }

   input->Script = NULL;
   input->Mode   = 0;

   STEP();
   return 0;
}

//****************************************************************************
// Input garbage collecter.

static int input_destruct(lua_State *Lua)
{
   struct finput *input;
   if ((input = (struct finput *)lua_touserdata(Lua, 1))) {
      FMSG("~input.destroy()","Surface: %d, CallbackRef: %d, KeyEvent: %p", input->SurfaceID, input->Callback, input->KeyEvent);

      if (input->SurfaceID) {
         // NB: If a keyboard subscription was created, the Display module may not be present/necessary.
         if (modDisplay) gfxUnsubscribeInput(input->SurfaceID);
         input->SurfaceID = 0;
      }

      if (input->InputObject) { luaL_unref(Lua, LUA_REGISTRYINDEX, input->InputObject); input->InputObject = 0; }
      if (input->Callback) { luaL_unref(Lua, LUA_REGISTRYINDEX, input->Callback); input->Callback = 0; }
      if (input->KeyEvent) { UnsubscribeEvent(input->KeyEvent); input->KeyEvent = NULL; }

      // Remove from the chain.

      if (Lua->Script) {
         struct prvFluid *prv = Lua->Script->Head.ChildPrivate;
         if (prv->InputList IS input) prv->InputList = input->Next;
         else {
            struct finput *list = prv->InputList;
            while (list) {
               if (list->Next IS input) {
                  list->Next = input->Next;
                  break;
               }
               list = list->Next;
            }
         }
      }

      STEP();
   }

   return 0;
}

//****************************************************************************
// Key events should only be received when a monitored surface has the focus.

static void key_event(struct finput *Input, evKey *Event, LONG Size)
{
   struct prvFluid *prv;

   if ((!Input->Script) OR (!(prv = Input->Script->Head.ChildPrivate))) {
      MSG("Input->Script undefined.");
      return;
   }

   FMSG("~key_event","Incoming keyboard input");

   LONG depth = GetResource(RES_LOG_DEPTH); // Required because thrown errors cause the debugger to lose its step position

      LONG top = lua_gettop(prv->Lua);
      lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, Input->Callback); // Get the function reference in Lua and place it on the stack
      lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, Input->InputObject); // Arg: Input Object
      lua_pushinteger(prv->Lua, Input->SurfaceID);  // Arg: Surface (if applicable)
      lua_pushinteger(prv->Lua, Event->Qualifiers); // Arg: Key Flags
      lua_pushinteger(prv->Lua, Event->Code);       // Arg: Key Value
      lua_pushinteger(prv->Lua, Event->Unicode);    // Arg: Unicode character

      if (lua_pcall(prv->Lua, 5, 0, 0)) {
         process_error(Input->Script, "Keyboard event callback");
      }

      lua_settop(prv->Lua, top);

   SetResource(RES_LOG_DEPTH, depth);

   FMSG("~key_event","Collecting garbage.");
     lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
   STEP();

   STEP();
}

//****************************************************************************
// This is a global function for monitoring the focus of surfaces that we want to filter on for keyboard input.

static void focus_event(lua_State *Lua, evFocus *Event, LONG Size)
{
   struct prvFluid *prv;
   if ((!Lua->Script) OR (!(prv = Lua->Script->Head.ChildPrivate))) {
      MSG("Script undefined.");
      return;
   }

   FMSG("~focus_event","Incoming focus event.");

   struct finput *input;
   for (input=prv->InputList; input; input=input->Next) {
      if (input->Mode != FIM_KEYBOARD) continue;
      if (input->KeyEvent) continue;

      LONG i;
      for (i=0; i < Event->TotalWithFocus; i++) {
         if (input->SurfaceID IS Event->FocusList[i]) {
            FUNCTION callback;
            SET_FUNCTION_STDC(callback, &key_event);
            FMSG("Fluid","Focus notification received for key events on surface #%d.", input->SurfaceID);
            SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, input, &input->KeyEvent);
            break;
         }
      }
   }

   for (input=prv->InputList; input; input=input->Next) {
      if (input->Mode != FIM_KEYBOARD) continue;
      if (!input->KeyEvent) continue;

      LONG i;
      for (i=0; i < Event->TotalLostFocus; i++) {
         if (input->SurfaceID IS Event->FocusList[Event->TotalWithFocus+i]) {
            FMSG("Fluid","Lost focus notification received for key events on surface #%d.", input->SurfaceID);
            UnsubscribeEvent(input->KeyEvent);
            input->KeyEvent = NULL;
            break;
         }
      }
   }

   STEP();
}

//****************************************************************************

static int input_tostring(lua_State *Lua)
{
   struct finput *input;
   if ((input = (struct finput *)lua_touserdata(Lua, 1))) {
      lua_pushfstring(Lua, "Input handler for surface #%d", input->SurfaceID);
   }
   else lua_pushstring(Lua, "?");

   return 1;
}

//****************************************************************************

static void register_input_class(lua_State *Lua)
{
   static const struct luaL_reg inputlib_functions[] = {
      { "subscribe",   input_subscribe },
      { "keyboard",    input_keyboard },
      { NULL, NULL }
   };

   static const struct luaL_reg inputlib_methods[] = {
      { "__gc",       input_destruct },
      { "__tostring", input_tostring },
      { "__index",    input_index },
      { NULL, NULL }
   };

   MSG("Registering input interface.");

   luaL_newmetatable(Lua, "Fluid.input");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, NULL, inputlib_methods, 0);
   luaL_openlib(Lua, "input", inputlib_functions, 0);
}
