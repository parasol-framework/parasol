// Action jump table implementation.  Actions will call the generic object_action_call() unless they have a direct
// implementation written for them.

inline void report_action_error(lua_State *Lua, struct object *Object, CSTRING Action, ERR Error)
{
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   if ((Error >= ERR::ExceptionThreshold) and (prv->Catch)) {
      char msg[180];
      prv->CaughtError = Error;
      snprintf(msg, sizeof(msg), "%s.%s() failed: %s", Object->Class->ClassName, Action, GetErrorMsg(Error));
      luaL_error(prv->Lua, msg);
   }
}

//********************************************************************************************************************

static int action_activate(lua_State *Lua)
{
   auto object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
   ERR error = ERR::Okay;
   bool release = false;

   if (object->DelayCall) {
      error = QueueAction(AC_Activate, object->UID, NULL);
      object->DelayCall = false;
   }
   else if (object->ObjectPtr) error = Action(AC_Activate, object->ObjectPtr, NULL);
   else if (auto obj = access_object(object)) {
      error = Action(AC_Activate, obj, NULL);
      release = true;
   }

   lua_pushinteger(Lua, LONG(error));
   if (release) release_object(object);
   report_action_error(Lua, object, "Activate", error);
   return 1;
}

static int action_draw(lua_State *Lua)
{
   auto object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");

   ERR error = ERR::Okay;
   BYTE argbuffer[sizeof(struct acDraw)+8]; // +8 for overflow protection in build_args()

   if ((error = build_args(Lua, glActions[AC_Draw].Args, glActions[AC_Draw].Size, argbuffer, NULL)) != ERR::Okay) {
      luaL_error(Lua, "Argument build failed for Draw().");
      return 0;
   }

   bool release = false;
   if (object->DelayCall) {
      error = QueueAction(AC_Draw, object->UID, argbuffer);
      object->DelayCall = false;
   }
   else if (object->ObjectPtr) error = Action(AC_Draw, object->ObjectPtr, argbuffer);
   else if (auto obj = access_object(object)) {
      error = Action(AC_Draw, obj, argbuffer);
      release = true;
   }

   lua_pushinteger(Lua, LONG(error));
   if (release) release_object(object);
   report_action_error(Lua, object, "Draw", error);
   return 1;
}

//********************************************************************************************************************

static int obj_jump_empty(lua_State *Lua, const obj_read &Handle, object *def) { return 0; }
static int obj_jump_signal(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Signal); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_activate(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushcclosure(Lua, action_activate, 1); return 1; }
static int obj_jump_clear(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Clear); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_copydata(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_CopyData); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_datafeed(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_DataFeed); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_deactivate(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Deactivate); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_draw(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushcclosure(Lua, action_draw, 1); return 1; }
static int obj_jump_flush(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Flush); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_focus(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Focus); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_savesettings(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_SaveSettings); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_getkey(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_GetKey); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_dragdrop(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_DragDrop); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_hide(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Hide); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_lock(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Lock); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_lostfocus(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_LostFocus); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_move(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Move); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_movetoback(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_MoveToBack); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_movetofront(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_MoveToFront); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_redo(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Redo); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_query(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Query); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_read(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Read); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_rename(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Rename); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_reset(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Reset); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_resize(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Resize); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_saveimage(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_SaveImage); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_savetoobject(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_SaveToObject); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_seek(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Seek); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_setkey(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_SetKey); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_show(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Show); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_undo(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Undo); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_unlock(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Unlock); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_next(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Next); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_prev(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Prev); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_write(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Write); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_setfield(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_SetField); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_clipboard(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Clipboard); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_refresh(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Refresh); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_disable(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Disable); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_enable(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Enable); lua_pushcclosure(Lua, object_action_call, 2); return 1; }
static int obj_jump_redimension(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_Redimension); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }
static int obj_jump_movetopoint(lua_State *Lua, const obj_read &Handle, object *def) { lua_pushvalue(Lua, 1); lua_pushinteger(Lua, AC_MoveToPoint); lua_pushcclosure(Lua, object_action_call_args, 2); return 1; }

static std::array<obj_read::JUMP *, AC_END> glJumpActions = {
   obj_jump_empty,
   obj_jump_signal,
   obj_jump_activate,
   obj_jump_redimension,
   obj_jump_clear,
   obj_jump_empty, // FreeWarning
   obj_jump_enable,
   obj_jump_copydata,
   obj_jump_datafeed,
   obj_jump_deactivate,
   obj_jump_draw,
   obj_jump_flush,
   obj_jump_focus,
   obj_jump_empty, // Free
   obj_jump_savesettings,
   obj_jump_getkey,
   obj_jump_dragdrop,
   obj_jump_hide,
   obj_jump_empty, // Init
   obj_jump_lock,
   obj_jump_lostfocus,
   obj_jump_move,
   obj_jump_movetoback,
   obj_jump_movetofront,
   obj_jump_empty, // NewChild
   obj_jump_empty, // NewOwner
   obj_jump_empty, // NewObject
   obj_jump_redo,
   obj_jump_query,
   obj_jump_read,
   obj_jump_rename,
   obj_jump_reset,
   obj_jump_resize,
   obj_jump_saveimage,
   obj_jump_savetoobject,
   obj_jump_movetopoint,
   obj_jump_seek,
   obj_jump_setkey,
   obj_jump_show,
   obj_jump_undo,
   obj_jump_unlock,
   obj_jump_next,
   obj_jump_prev,
   obj_jump_write,
   obj_jump_setfield,
   obj_jump_clipboard,
   obj_jump_refresh,
   obj_jump_disable   
};
