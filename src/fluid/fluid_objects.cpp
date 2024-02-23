/*

Some notes about object ownership and the safe handling of them:

The core's technical design means that any object that is *not directly owned by the Lua Script* must be treated as
external to that script.  External objects must be locked appropriately whenever they are used.  Locking
ensures that threads can interact with the object safely and that the object cannot be prematurely terminated.

Only objects created through the standard obj.new() interface are directly accessible without a lock.  Those referenced
through obj.find(), push_object(), or children created with some_object.new() are marked as detached.

*/

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <inttypes.h>

extern "C" {
 #include "lua.h"
 #include "lualib.h"
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

template<class... Args> void RMSG(Args...) {
   //log.trace(Args)  // Enable if you want to debug results returned from functions, actions etc
}

static ULONG OJH_init, OJH_free, OJH_lock, OJH_children, OJH_detach, OJH_get, OJH_new, OJH_state, OJH_var, OJH_getVar;
static ULONG OJH_set, OJH_setVar, OJH_delayCall, OJH_exists, OJH_subscribe, OJH_unsubscribe;

static int object_action_call_args(lua_State *);
static int object_method_call_args(lua_State *);
static int object_action_call(lua_State *);
static int object_method_call(lua_State *);
static LONG get_results(lua_State *, const FunctionField *, const BYTE *);
static ERROR set_object_field(lua_State *, OBJECTPTR, CSTRING, LONG);

static int object_children(lua_State *);
static int object_delaycall(lua_State *);
static int object_detach(lua_State *);
static int object_exists(lua_State *);
static int object_free(lua_State *);
static int object_get(lua_State *);
static int object_getvar(lua_State *);
static int object_init(lua_State *);
static int object_lock(lua_State *);
static int object_newchild(lua_State *);
static int object_newindex(lua_State *);
static int object_set(lua_State *);
static int object_setvar(lua_State *);
static int object_state(lua_State *);
static int object_subscribe(lua_State *);
static int object_unsubscribe(lua_State *);

static int object_get_id(lua_State *, const obj_read &, object *);
static int object_get_rgb(lua_State *, const obj_read &, object *);
static int object_get_array(lua_State *, const obj_read &, object *);
static int object_get_struct(lua_State *, const obj_read &, object *);
static int object_get_string(lua_State *, const obj_read &, object *);
static int object_get_object(lua_State *, const obj_read &, object *);
static int object_get_ptr(lua_State *, const obj_read &, object *);
static int object_get_double(lua_State *, const obj_read &, object *);
static int object_get_large(lua_State *, const obj_read &, object *);
static int object_get_ulong(lua_State *, const obj_read &, object *);
static int object_get_long(lua_State *, const obj_read &, object *);

static ERROR object_set_array(lua_State *, OBJECTPTR, Field *, LONG);
static ERROR object_set_function(lua_State *, OBJECTPTR, Field *, LONG);
static ERROR object_set_object(lua_State *, OBJECTPTR, Field *, LONG);
static ERROR object_set_ptr(lua_State *, OBJECTPTR, Field *, LONG);
static ERROR object_set_double(lua_State *, OBJECTPTR, Field *, LONG);
static ERROR object_set_lookup(lua_State *, OBJECTPTR, Field *, LONG);
static ERROR object_set_oid(lua_State *, OBJECTPTR, Field *, LONG);
static ERROR object_set_number(lua_State *, OBJECTPTR, Field *, LONG);

//********************************************************************************************************************

#include "fluid_object_actions.cpp"

static std::unordered_map<objMetaClass *, std::set<obj_read, decltype(read_hash)>> glClassReadTable;
static std::unordered_map<objMetaClass *, std::set<obj_write, decltype(write_hash)>> glClassWriteTable;

inline void SET_CONTEXT(lua_State *Lua, APTR Function) {
   lua_pushvalue(Lua, 1); // Duplicate the object reference
   lua_pushcclosure(Lua, (lua_CFunction)Function, 1); // C function to call, +1 value for the object reference
}

static int stack_object_children(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_children); return 1; }
static int stack_object_delayCall(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_delaycall); return 1; }
static int stack_object_detach(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_detach); return 1; }
static int stack_object_exists(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_exists); return 1; }
static int stack_object_free(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_free); return 1; }
static int stack_object_get(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_get); return 1; }
static int stack_object_getVar(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_getvar); return 1; }
static int stack_object_init(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_init); return 1; }
static int stack_object_lock(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_lock); return 1; }
static int stack_object_newchild(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_newchild); return 1; }
static int stack_object_set(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_set); return 1; }
static int stack_object_setVar(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_setvar); return 1; }
static int stack_object_state(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_state); return 1; }
static int stack_object_subscribe(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_subscribe); return 1; }
static int stack_object_unsubscribe(lua_State *Lua, const obj_read &Handle, object *def) { SET_CONTEXT(Lua, (APTR)object_unsubscribe); return 1; }

//********************************************************************************************************************

static int obj_jump_method(lua_State *Lua, const obj_read &Handle, object *def)
{
   lua_pushvalue(Lua, 1); // Arg1: Duplicate the object reference
   lua_pushlightuserdata(Lua, Handle.Data); // Arg2: Method lookup table
   if ((((MethodEntry *)Handle.Data)->Args) and (((MethodEntry *)Handle.Data)->Size)) {
      lua_pushcclosure(Lua, object_method_call_args, 2); // Push a c closure with 2 input values on the stack
   }
   else lua_pushcclosure(Lua, object_method_call, 2);
   return 1;
}

//********************************************************************************************************************

inline READ_TABLE * get_read_table(object *Def)
{
   if (!Def->ReadTable) {
      if (auto it = glClassReadTable.find(Def->Class); it != glClassReadTable.end()) {
         Def->ReadTable = &it->second;
      }
      else {
         READ_TABLE jmp;

         // Every possible action is hashed because both sub-class and base-class actions require support.

         for (LONG code=1; code < AC_END; code++) {
            auto hash = simple_hash(glActions[code].Name, simple_hash("ac"));
            jmp.insert(obj_read(hash, glJumpActions[code]));
         }

         MethodEntry *methods;
         LONG total_methods;
         if (!GetFieldArray(Def->Class, FID_Methods, &methods, &total_methods)) {
            for (LONG i=1; i < total_methods; i++) {
               if (methods[i].MethodID) {
                  auto hash = simple_hash(methods[i].Name, simple_hash("mt"));
                  jmp.insert(obj_read(hash, obj_jump_method, &methods[i]));
               }
            }
         }

         Field *dict;
         LONG total_dict;
         if (!GetFieldArray(Def->Class, FID_Dictionary, &dict, &total_dict)) {
            jmp.insert(obj_read(simple_hash("id"), object_get_id));

            for (LONG i=0; i < total_dict; i++) {
               if (dict[i].Flags & FDF_R) {
                  char ch[2] = { dict[i].Name[0], 0 };
                  if ((ch[0] >= 'A') and (ch[0] <= 'Z')) ch[0] = ch[0] - 'A' + 'a';
                  auto hash = simple_hash(dict[i].Name+1, simple_hash(ch));

                  if (dict[i].Flags & FD_ARRAY) {
                     if (dict[i].Flags & FD_RGB) {
                        jmp.insert(obj_read(hash, object_get_rgb, &dict[i]));
                     }
                     else jmp.insert(obj_read(hash, object_get_array, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_STRUCT) {
                     jmp.insert(obj_read(hash, object_get_struct, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_STRING) {
                     jmp.insert(obj_read(hash, object_get_string, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_POINTER) {
                     if (dict[i].Flags & (FD_OBJECT|FD_INTEGRAL)) { // Writing to an integral is permitted if marked as writeable.
                        jmp.insert(obj_read(hash, object_get_object, &dict[i]));
                     }
                     else jmp.insert(obj_read(hash, object_get_ptr, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_DOUBLE) {
                     jmp.insert(obj_read(hash, object_get_double, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_LARGE) {
                     jmp.insert(obj_read(hash, object_get_large, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_LONG) {
                     if (dict[i].Flags & FD_UNSIGNED) {
                        jmp.insert(obj_read(hash, object_get_ulong, &dict[i]));
                     }
                     else jmp.insert(obj_read(hash, object_get_long, &dict[i]));
                  }
               }
            }
         }

         jmp.emplace(OJH_init, stack_object_init);
         jmp.emplace(OJH_free, stack_object_free);
         jmp.emplace(OJH_lock, stack_object_lock);
         jmp.emplace(OJH_children, stack_object_children);
         jmp.emplace(OJH_detach, stack_object_detach);
         jmp.emplace(OJH_get, stack_object_get);
         jmp.emplace(OJH_new, stack_object_newchild);
         jmp.emplace(OJH_state, stack_object_state);
         jmp.emplace(OJH_var, stack_object_getVar);
         jmp.emplace(OJH_getVar, stack_object_getVar);
         jmp.emplace(OJH_set, stack_object_set);
         jmp.emplace(OJH_setVar, stack_object_setVar);
         jmp.emplace(OJH_delayCall, stack_object_delayCall);
         jmp.emplace(OJH_exists, stack_object_exists);
         jmp.emplace(OJH_subscribe, stack_object_subscribe);
         jmp.emplace(OJH_unsubscribe, stack_object_unsubscribe);

         glClassReadTable[Def->Class] = std::move(jmp);
         Def->ReadTable = &glClassReadTable[Def->Class];
      }
   }
   return Def->ReadTable;
}

//********************************************************************************************************************

inline WRITE_TABLE * get_write_table(object *Def)
{
   if (!Def->WriteTable) {
      if (auto it = glClassWriteTable.find(Def->Class); it != glClassWriteTable.end()) {
         Def->WriteTable = &it->second;
      }
      else {
         WRITE_TABLE jmp;
         Field *dict;
         LONG total_dict;
         if (!GetFieldArray(Def->Class, FID_Dictionary, &dict, &total_dict)) {
            for (LONG i=0; i < total_dict; i++) {
               if (dict[i].Flags & (FD_W|FD_I)) {
                  char ch[2] = { dict[i].Name[0], 0 };
                  if ((ch[0] >= 'A') and (ch[0] <= 'Z')) ch[0] = ch[0] - 'A' + 'a';
                  auto hash = simple_hash(dict[i].Name+1, simple_hash(ch));

                  if (dict[i].Flags & FD_ARRAY) {
                     jmp.insert(obj_write(hash, object_set_array, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_FUNCTION) {
                     jmp.insert(obj_write(hash, object_set_function, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_POINTER) {
                     if (dict[i].Flags & (FD_OBJECT|FD_INTEGRAL)) {
                        jmp.insert(obj_write(hash, object_set_object, &dict[i]));
                     }
                     else jmp.insert(obj_write(hash, object_set_ptr, &dict[i]));
                  }
                  else if (dict[i].Flags & (FD_DOUBLE|FD_FLOAT)) {
                     jmp.insert(obj_write(hash, object_set_double, &dict[i]));
                  }
                  else if (dict[i].Flags & (FD_FLAGS|FD_LOOKUP)) {
                     jmp.insert(obj_write(hash, object_set_lookup, &dict[i]));
                  }
                  else if (dict[i].Flags & FD_OBJECT) { // Object ID
                     jmp.insert(obj_write(hash, object_set_oid, &dict[i]));
                  }
                  else if (dict[i].Flags & (FD_LONG|FD_LARGE)) {
                     jmp.insert(obj_write(hash, object_set_number, &dict[i]));
                  }
               }
            }
         }

         glClassWriteTable[Def->Class] = std::move(jmp);
         Def->WriteTable = &glClassWriteTable[Def->Class];
      }
   }
   return Def->WriteTable;
}

//********************************************************************************************************************
// Any Read accesses to the object will pass through here.  The requested key must exist in the hashed jump-table for
// the targeted class, or an error will be returned.

static int object_index(lua_State *Lua)
{
   if (auto def = (object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      auto keyname = luaL_checkstring(Lua, 2);
      auto jt  = get_read_table(def);

      if (auto func = jt->find(obj_read(simple_hash(keyname))); func != jt->end()) {
         return func->Call(Lua, *func, def);
      }
      else {
         pf::Log log(__FUNCTION__);
         log.warning("Unable to read %s.%s", def->Class->ClassName, keyname);
         auto prv = (prvFluid *)Lua->Script->ChildPrivate;
         prv->CaughtError = ERR_NoSupport;
         //if (prv->ThrowErrors) luaL_error(Lua, GetErrorMsg);
      }
   }

   return 0;
}

//********************************************************************************************************************

static ACTIONID get_action_info(lua_State *Lua, CLASSID ClassID, CSTRING action, const FunctionField **Args)
{
   pf::Log log;

   if ((action[0] IS 'm') and (action[1] IS 't')) { // User is explicitly referring to a method
      action += 2;
   }
   else {
      auto it = glActionLookup.find(action);
      if (it != glActionLookup.end()) {
         *Args = glActions[it->second].Args;
         return it->second;
      }
   }

   *Args = NULL;
   if (auto mc = FindClass(ClassID)) {
      MethodEntry *table;
      LONG total_methods;
      ACTIONID action_id;
      if ((!GetFieldArray(mc, FID_Methods, &table, &total_methods)) and (table)) {
         for (LONG i=1; i < total_methods; i++) {
            if ((table[i].Name) and (!StrMatch(action, table[i].Name))) {
               action_id = table[i].MethodID;
               *Args = table[i].Args;
               i = 0x7ffffff0;
               return action_id;
            }
         }
      }
      else log.warning("No methods declared for class %s, cannot call %s()", mc->ClassName, action);
   }
   else luaL_error(Lua, GetErrorMsg(ERR_Search));

   return 0;
}

/*********************************************************************************************************************
** Usage: object = obj.new("Display", { field1 = value1, field2 = value2, ...})
**
** If fields are provided in the second argument, the object will be initialised automatically.  If no field list is
** provided, InitObject() must be used to initialise the object.
**
** Variable fields can be denoted with an underscore prefix.
**
** Also see object_newchild() for creating objects from a parent.
**
** Errors are immediately thrown.
*/

static int object_new(lua_State *Lua)
{
   pf::Log log("obj.new");
   CSTRING class_name;
   CLASSID class_id;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   NF objflags = NF::NIL;
   LONG type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      class_id = lua_tointeger(Lua, 1);
      class_name = NULL;
      log.trace("$%.8x", class_id);
   }
   else if ((class_name = luaL_checkstring(Lua, 1))) {
      if (class_name[0] IS '@') { // Deprecated
         log.warning("Use of @ for allocating public objects is deprecated.");
         class_name++;
      }
      class_id = StrHash(class_name, 0);
      log.trace("%s, $%.8x", class_name, class_id);
   }
   else {
      log.warning("String or ID expected for class name, got '%s'.", lua_typename(Lua, type));
      prv->CaughtError = ERR_Mismatch;
      luaL_error(Lua, GetErrorMsg(ERR_Mismatch));
      return 0;
   }

   OBJECTPTR obj;
   ERROR error;
   if (!(error = NewObject(class_id, objflags, &obj))) {
      if (Lua->Script->TargetID) obj->set(FID_Owner, Lua->Script->TargetID);

      obj->CreatorMeta = Lua;

      auto_load_include(Lua, obj->Class);

      auto def = (object *)lua_newuserdata(Lua, sizeof(object));
      ClearMemory(def, sizeof(object));

      luaL_getmetatable(Lua, "Fluid.obj");
      lua_setmetatable(Lua, -2);
      if (lua_istable(Lua, 2)) {
         // Set fields against the object and initialise the object.  NOTE: Lua's table management code *does not*
         // preserve the order in which the fields were originally passed to the table.

         ERROR field_error  = ERR_Okay;
         CSTRING field_name = NULL;
         LONG failed_type   = LUA_TNONE;
         lua_pushnil(Lua);  // Access first key for lua_next()
         while (lua_next(Lua, 2) != 0) {
            if ((field_name = luaL_checkstring(Lua, -2))) {
               if (!StrMatch("owner", field_name)) field_error = ERR_UnsupportedOwner; // Setting the owner is not permitted.
               else field_error = set_object_field(Lua, obj, field_name, -1);
            }
            else field_error = ERR_UnsupportedField;

            if (field_error) { // Break the loop early on error.
               failed_type = lua_type(Lua, -1);
               lua_pop(Lua, 2);
               break;
            }
            else lua_pop(Lua, 1);  // removes 'value'; keeps 'key' for the proceeding lua_next() iteration
         }

         if ((field_error) or ((error = InitObject(obj)) != ERR_Okay)) {
            FreeResource(obj);

            if (field_error) {
               prv->CaughtError = field_error;
               luaL_error(Lua, "Failed to set field '%s.%s' with %s, error: %s", class_name, field_name, lua_typename(Lua, failed_type), GetErrorMsg(field_error));
            }
            else {
               log.warning("Failed to Init() object '%s', error: %s", class_name, GetErrorMsg(error));
               prv->CaughtError = error;
               luaL_error(Lua, GetErrorMsg(error));
            }
            return 0;
         }
      }

      def->ObjectPtr = obj; // Defining ObjectPtr ensures maximum efficiency in access_object()
      def->UID       = obj->UID;
      def->Class     = obj->Class;
      return 1;
   }
   else {
      prv->CaughtError = ERR_NewObject;
      luaL_error(Lua, "NewObject() failed for class '%s', error: %s", class_name, GetErrorMsg(error));
      return 0;
   }
}

//********************************************************************************************************************
// Usage: state = some_object.state()
//
// Returns a table that can be used to store information that is specific to the object.  The state is linked to the
// object ID to ensure that the state values are still accessible if referenced elsewhere in the script.

static int object_state(lua_State *Lua)
{
   object *def;
   if (!(def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   // Note: At this time no cleanup is performed on the StateMap.  Ideally this would be done with a hook into garbage
   // collection cycles.

   pf::Log log(__FUNCTION__);
   auto it = prv->StateMap.find(def->UID);
   if (it != prv->StateMap.end()) {
      lua_rawgeti(Lua, LUA_REGISTRYINDEX, it->second);
      return 1;
   }
   else {
      lua_createtable(Lua, 0, 0); // Create a new table on the stack.
      auto state_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);
      prv->StateMap[def->UID] = state_ref;
      lua_rawgeti(Lua, LUA_REGISTRYINDEX, state_ref);
      return 1;
   }
}

//********************************************************************************************************************
// Syntactic sugar for creating new objects against a parent, e.g. window.new("button", { ... }).  Behaviour is
// mostly identical to obj.new() but the object is detached.

static int object_newchild(lua_State *Lua)
{
   pf::Log log("obj.child");
   object *parent;

   if (!(parent = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   CSTRING class_name;
   CLASSID class_id;
   NF objflags = NF::NIL;
   LONG type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      class_id = lua_tointeger(Lua, 1);
      class_name = NULL;
      log.trace("$%.8x", class_id);
   }
   else if ((class_name = luaL_checkstring(Lua, 1))) {
      if (class_name[0] IS '@') { // Deprecated
         class_name++;
         log.warning("Use of @ for allocating public objects is deprecated.");
      }
      class_id = StrHash(class_name, 0);
      log.trace("%s, $%.8x", class_name, class_id);
   }
   else {
      log.warning("String or ID expected for class name, got '%s'.", lua_typename(Lua, type));
      prv->CaughtError = ERR_Mismatch;
      luaL_error(Lua, GetErrorMsg(ERR_Mismatch));
      return 0;
   }

   OBJECTPTR obj;
   ERROR error;
   if (!(error = NewObject(class_id, objflags, &obj))) {
      if (Lua->Script->TargetID) obj->set(FID_Owner, Lua->Script->TargetID);

      obj->CreatorMeta = Lua;

      auto_load_include(Lua, obj->Class);

      auto def = (object *)lua_newuserdata(Lua, sizeof(object));
      ClearMemory(def, sizeof(object));

      luaL_getmetatable(Lua, "Fluid.obj");
      lua_setmetatable(Lua, -2);

      lua_pushinteger(Lua, parent->UID); // ID of the would-be parent.
      set_object_field(Lua, obj, "owner", lua_gettop(Lua));
      lua_pop(Lua, 1);

      if (lua_istable(Lua, 2)) {
         // Set fields against the object and initialise the object.  NOTE: Lua's table management code *does not*
         // preserve the order in which the fields were originally passed to the table.

         ERROR field_error = ERR_Okay;
         CSTRING field_name = NULL;
         lua_pushnil(Lua);  // Access first key for lua_next()
         while (lua_next(Lua, 2) != 0) {
            if ((field_name = luaL_checkstring(Lua, -2))) {
               if (!StrMatch("owner", field_name)) field_error = ERR_UnsupportedOwner; // Setting the owner is not permitted.
               else field_error = set_object_field(Lua, obj, field_name, -1);
            }
            else field_error = ERR_UnsupportedField;

            if (field_error) { // Break the loop early on error.
               lua_pop(Lua, 2);
               break;
            }
            else lua_pop(Lua, 1);  // removes 'value'; keeps 'key' for the proceeding lua_next() iteration
         }

         if ((field_error) or ((error = InitObject(obj)) != ERR_Okay)) {
            FreeResource(obj);

            if (field_error) {
               prv->CaughtError = field_error;
               luaL_error(Lua, "Failed to set field '%s', error: %s", field_name, GetErrorMsg(field_error));
            }
            else {
               log.warning("Failed to Init() object '%s', error: %s", class_name, GetErrorMsg(error));
               prv->CaughtError = ERR_Init;
               luaL_error(Lua, GetErrorMsg(ERR_Init));
            }
            return 0;
         }
      }

      def->ObjectPtr = NULL; // Objects created as children are treated as detached.
      def->Detached  = true;
      def->UID       = obj->UID;
      def->Class     = obj->Class;
      return 1;
   }
   else {
      prv->CaughtError = ERR_NewObject;
      luaL_error(Lua, GetErrorMsg(ERR_NewObject));
      return 0;
   }
}

//********************************************************************************************************************
// Throws exceptions.  Used for returning objects to the user.

object * push_object(lua_State *Lua, OBJECTPTR Object)
{
   if (auto newobject = (object *)lua_newuserdata(Lua, sizeof(object))) {
      ClearMemory(newobject, sizeof(object));

      auto_load_include(Lua, Object->Class);

      newobject->ObjectPtr = NULL;
      newobject->UID       = Object->UID;
      newobject->Class     = Object->Class;
      newobject->Detached  = true; // Object is not linked to this Lua value.

      luaL_getmetatable(Lua, "Fluid.obj");
      lua_setmetatable(Lua, -2);
      return newobject;
   }
   else luaL_error(Lua, "Failed to create new object.");
   return NULL;
}

//********************************************************************************************************************
// Guaranteed to not throw exceptions.

ERROR push_object_id(lua_State *Lua, OBJECTID ObjectID)
{
   if (!ObjectID) { lua_pushnil(Lua); return ERR_Okay; }

   if (auto newobject = (object *)lua_newuserdata(Lua, sizeof(object))) {
      ClearMemory(newobject, sizeof(object));

      if (auto object = GetObjectPtr(ObjectID)) {
         newobject->UID = ObjectID;
         newobject->Class    = object->Class;
      }

      newobject->Detached  = true;

      luaL_getmetatable(Lua, "Fluid.obj");
      lua_setmetatable(Lua, -2);
      return ERR_Okay;
   }
   else return ERR_NewObject;
}

/*********************************************************************************************************************
** Usage: object = obj.find("ObjectName" | ObjectID, [ClassName | ClassID])
**
** Returns nil on error - does not throw exceptions.
**
** The fluid object itself can be found by using the name "self".
*/

static int object_find_ptr(lua_State *Lua, OBJECTPTR obj)
{
   // Private objects discovered by obj.find() have to be treated as an external reference at all times
   // (access must controlled by access_object() and release_object() calls).

   auto_load_include(Lua, obj->Class);

   auto def = (object *)lua_newuserdata(Lua, sizeof(object)); // +1 stack
   ClearMemory(def, sizeof(object));
   luaL_getmetatable(Lua, "Fluid.obj"); // +1 stack
   lua_setmetatable(Lua, -2); // -1 stack

   def->ObjectPtr   = NULL;
   def->UID         = obj->UID;
   def->Class       = obj->Class;
   def->Detached    = true;
   return 1;
}

static int object_find(lua_State *Lua)
{
   pf::Log log("object.find");
   OBJECTPTR obj;
   CSTRING object_name;
   CLASSID class_id;
   OBJECTID object_id;

   LONG type = lua_type(Lua, 1);
   if ((type IS LUA_TSTRING) and ((object_name = lua_tostring(Lua, 1)))) {
      LONG class_type = lua_type(Lua, 2); // Optional
      if (class_type IS LUA_TNUMBER) {
         class_id = lua_tointeger(Lua, 2);
      }
      else if (class_type IS LUA_TSTRING) {
         class_id = StrHash(lua_tostring(Lua, 2), false);
      }
      else class_id = 0;

      log.trace("obj.find(%s, $%.8x)", object_name, class_id);

      if ((!StrMatch("self", object_name)) and (!class_id)) {
         return object_find_ptr(Lua, Lua->Script);
      }
      else if (!StrMatch("owner", object_name)) {
         if (auto obj = Lua->Script->Owner) {
            return object_find_ptr(Lua, obj);
         }
         else return 0;
      }

      if (!FindObject(object_name, class_id, FOF::SMART_NAMES, &object_id)) {
         return object_find_ptr(Lua, GetObjectPtr(object_id));
      }
      else log.debug("Unable to find object '%s'", object_name);
   }
   else if ((type IS LUA_TNUMBER) and ((object_id = lua_tointeger(Lua, 1)))) {
      log.trace("obj.find(#%d)", object_id);

      if (CheckObjectExists(object_id) != ERR_Okay) return 0;

      if (auto obj = Lua->Script->Owner) {
         return object_find_ptr(Lua, obj);
      }
   }
   else log.warning("String or ID expected for object name, got '%s'.", lua_typename(Lua, type));

   return 0;
}

//********************************************************************************************************************
// Usage: metaclass = obj.class(object)
//
// Returns the MetaClass for an object, representing it as an inspectable object.

static int object_class(lua_State *Lua)
{
   object *query;
   if (!(query = (object *)get_meta(Lua, 1, "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   objMetaClass *cl = query->Class;
   auto def = (object *)lua_newuserdata(Lua, sizeof(object)); // +1 stack
   ClearMemory(def, sizeof(object));
   luaL_getmetatable(Lua, "Fluid.obj"); // +1 stack
   lua_setmetatable(Lua, -2); // -1 stack

   def->ObjectPtr = cl;
   def->UID       = cl->UID;
   def->Class     = cl;
   def->Detached  = true;
   return 1;
}

//********************************************************************************************************************
// Usage: obj.children(["ClassNameFilter"])
//
// Returns an object ID array of children belonging to the queried object.  If there are no children, an empty array is
// returned.

static int object_children(lua_State *Lua)
{
   pf::Log log("obj.children");

   log.trace("");

   object *def;
   if (!(def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   CLASSID class_id;
   CSTRING classfilter;
   if ((classfilter = luaL_optstring(Lua, 1, NULL)) and (classfilter[0])) {
      class_id = StrHash(classfilter, 0);
   }
   else class_id = 0;

   pf::vector<ChildEntry> list;
   if (!ListChildren(def->UID, &list)) {
      LONG index = 0;
      auto id = std::make_unique<LONG[]>(list.size());
      for (auto &rec : list) {
         if (class_id) {
            if (rec.ClassID IS class_id) id[index++] = rec.ObjectID;
         }
         else id[index++] = rec.ObjectID;
      }

      make_table(Lua, FD_LONG, index, id.get());
   }
   else make_table(Lua, FD_LONG, 0, NULL);

   return 1; // make_table() always returns a value even if it is nil
}

//********************************************************************************************************************
// obj:lock(function()
//    --Code--
// end)
//
// This method will lock the target object and then execute the function.  The lock will be released on the function's
// completion.

static int object_lock(lua_State *Lua)
{
   object *def;

   if (!(def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   if (!lua_isfunction(Lua, 1)) {
      luaL_argerror(Lua, 1, "Function expected.");
      return 0;
   }

   if (access_object(def)) {
      pf::Log log("obj.lock");
      log.branch("Object: %d", def->UID);
      lua_pcall(Lua, 0, 0, 0);
      release_object(def);
   }
   return 0;
}

//********************************************************************************************************************
// Usage: obj:detach()
//
// Detaches the object from the metatable, this stops the object from being killed on garbage collection.  HOWEVER: The
// object will still belong to the Script, so once that is freed, the object will go down with it.

static int object_detach(lua_State *Lua)
{
   object *def;

   if (!(def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   pf::Log log("obj.detach");
   log.traceBranch("Detached: %d", def->Detached);

   if (!def->Detached) def->Detached = true;

   return 0;
}

/*********************************************************************************************************************
** Usage: obj.exists()
**
** Returns true if the object still exists, otherwise nil.
*/

static int object_exists(lua_State *Lua)
{
   if (auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj")) {
      if (access_object(def)) {
         release_object(def);
         lua_pushboolean(Lua, true);
         return 1;
      }
      else return 0;
   }
   else return 0;
}

/*********************************************************************************************************************
** Usage: obj.subscribe(ActionName, Function, Reference)
**
** Subscribe a function to an action or method.  Throws an exception on failure.  The client feedback prototype is:
**
**    function(Object, Args, Reference)
*/

static int object_subscribe(lua_State *Lua)
{
   object *def;

   if (!(def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_error(Lua, "Expected object.");
      return 0;
   }

   CSTRING action;
   if (!(action = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Action name expected.");
      return 0;
   }

   if (!lua_isfunction(Lua, 2)) {
      luaL_argerror(Lua, 2, "Function expected.");
      return 0;
   }

   const FunctionField *arglist;
   ACTIONID action_id = get_action_info(Lua, def->Class->ClassID, action, &arglist);

   if (!action_id) {
      luaL_argerror(Lua, 1, "Action/Method name is invalid.");
      return 0;
   }

   OBJECTPTR obj;
   if (!(obj = access_object(def))) {
      luaL_error(Lua, GetErrorMsg(ERR_AccessObject));
      return 0;
   }

   pf::Log log("obj.subscribe");
   log.trace("Object: %d, Action: %s (ID %d)", def->UID, action, action_id);

   auto callback = FUNCTION(notify_action);
   callback.StdC.Context = Lua->Script;
   if (auto error = SubscribeAction(obj, action_id, &callback); !error) {
      auto prv = (prvFluid *)Lua->Script->ChildPrivate;
      auto &acsub = prv->ActionList.emplace_back();

      if (!lua_isnil(Lua, 3)) { // A custom reference for the callback can be specified in arg 3.
         lua_settop(prv->Lua, 3);
         acsub.Reference = luaL_ref(prv->Lua, LUA_REGISTRYINDEX); // Pops value from stack and returns it as a reference that can be used later.
      }
      else acsub.Reference = 0;

      lua_settop(prv->Lua, 2);
      acsub.Function = luaL_ref(prv->Lua, LUA_REGISTRYINDEX); // Pops value from stack and returns it as a reference that can be used later.
      acsub.Object   = def;
      acsub.Args     = arglist;
      acsub.ObjectID = def->UID;
      acsub.ActionID = action_id;

      release_object(def);
   }
   else {
      release_object(def);
      luaL_error(Lua, GetErrorMsg(error));
   }
   return 0;
}

//********************************************************************************************************************
// Usage: obj.unsubscribe(ActionName)

static int object_unsubscribe(lua_State *Lua)
{
   pf::Log log("unsubscribe");

   object *def;
   if (!(def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_error(Lua, "Expected object.");
      return 0;
   }

   CSTRING action;
   if (!(action = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Action name expected.");
      return 0;
   }

   const FunctionField *arglist;
   ACTIONID action_id = get_action_info(Lua, def->Class->ClassID, action, &arglist);

   if (!action_id) {
      luaL_argerror(Lua, 1, "Action/Method name is invalid.");
      return 0;
   }

   log.trace("Object: %d, Action: %s", def->UID, action);

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   for (auto it=prv->ActionList.begin(); it != prv->ActionList.end(); ) {
      if ((it->ObjectID IS def->UID) and
          ((!action_id) or (it->ActionID IS action_id))) {
         luaL_unref(Lua, LUA_REGISTRYINDEX, it->Function);
         if (it->Reference) luaL_unref(Lua, LUA_REGISTRYINDEX, it->Reference);
         it = prv->ActionList.erase(it);
         continue;
      }
      it++;
   }

   return 0;
}

//********************************************************************************************************************
// Usage: obj.delayCall()
//
// Delays the next action or method call that is taken against this object.

static int object_delaycall(lua_State *Lua)
{
   if (auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj")) def->DelayCall = true;
   else luaL_argerror(Lua, 1, "Expected object.");
   return 0;
}

//********************************************************************************************************************
// Object garbage collector.
//
// NOTE: It is possible for the referenced object to have already been destroyed if it is owned by something outside of
// Fluid's environment.  This is commonplace for UI objects.  In addition the object's class may have been removed if
// the termination process is running during an expunge.

static int object_destruct(lua_State *Lua)
{
   if (auto def = (object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      while (def->AccessCount > 0) release_object(def);

      if (!def->Detached) {
         // Note that if the object's owner has switched to something out of our context, we
         // don't terminate it (an exception is applied for Recordset objects as these must be
         // owned by a Database object).

         if (auto obj = GetObjectPtr(def->UID)) {
            if ((obj->Class->BaseClassID IS ID_RECORDSET) or (obj->Owner IS Lua->Script) or (obj->ownerID() IS Lua->Script->TargetID)) {
               pf::Log log("obj.destruct");
               log.trace("Freeing Fluid-owned object #%d.", def->UID);
               FreeResource(obj); // We can't presume that the object pointer would be valid
            }
         }
      }
   }

   return 0;
}

//********************************************************************************************************************

static int object_free(lua_State *Lua)
{
   if (auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj")) {
      FreeResource(def->UID);
      ClearMemory(def, sizeof(object)); // Mark the object as unusable
   }

   return 0;
}

//********************************************************************************************************************

static int object_init(lua_State *Lua)
{
   if (auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj")) {
      if (auto obj = access_object(def)) {
         lua_pushinteger(Lua, InitObject(obj));
         release_object(def);
         return 1;
      }
      else {
         luaL_error(Lua, GetErrorMsg(ERR_AccessObject));
         return 0;
      }
   }
   else {
      lua_pushinteger(Lua, ERR_SystemCorrupt);
      return 1;
   }
}

//********************************************************************************************************************
// Prints the object interface as the object ID, e.g. #-10513

static int object_tostring(lua_State *Lua)
{
   if (auto def = (object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      pf::Log log("obj.tostring");
      log.trace("#%d", def->UID);
      lua_pushfstring(Lua, "#%d", def->UID);
   }
   else lua_pushstring(Lua, "?");
   return 1;
}

//********************************************************************************************************************
// Support for pairs() allows the meta fields of the object to be iterated.  Note that in next_pair(), the object
// interface isn't used but could be pushed as an upvalue if needed.

static int object_next_pair(lua_State *Lua)
{
   auto fields = (Field *)lua_touserdata(Lua, lua_upvalueindex(1));
   LONG field_total = lua_tointeger(Lua, lua_upvalueindex(2));
   LONG field_index = lua_tointeger(Lua, lua_upvalueindex(3));

   if ((field_index >= 0) and (field_index < field_total)) {
      lua_pushinteger(Lua, field_index + 1);
      lua_replace(Lua, lua_upvalueindex(3)); // Update the field counter

      lua_pushstring(Lua, fields[field_index].Name);
      lua_pushinteger(Lua, fields[field_index].Flags);
      return 2;
   }
   else return 0; // Terminates the iteration
}

static int object_pairs(lua_State *Lua)
{
   if (auto def = (object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      Field *fields;
      LONG total;
      if (!GetFieldArray(def->Class, FID_Dictionary, &fields, &total)) {
         lua_pushlightuserdata(Lua, fields);
         lua_pushinteger(Lua, total);
         lua_pushinteger(Lua, 0);
         lua_pushcclosure(Lua, object_next_pair, 3);
         return 1;
      }
      else luaL_error(Lua, "Object class defines no fields.");
   }
   else luaL_error(Lua, "Expected object.");
   return 0;
}

//********************************************************************************************************************
// Similar to pairs(), but returns each field index and its name.

static int object_next_ipair(lua_State *Lua)
{
   auto fields = (Field *)lua_touserdata(Lua, lua_upvalueindex(1));
   LONG field_total = lua_tointeger(Lua, lua_upvalueindex(2));
   LONG field_index = lua_tointeger(Lua, 2); // Arg 2 is the previous index.  It's nil if this is the first iteration.

   if ((field_index >= 0) and (field_index < field_total)) {
      lua_pushinteger(Lua, field_index + 1);
      lua_pushstring(Lua, fields[field_index].Name);
      return 2;
   }
   else return 0; // Terminates the iteration
}

static int object_ipairs(lua_State *Lua)
{
   if (auto def = (object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      Field *fields;
      LONG total;
      if (!GetFieldArray(def->Class, FID_Dictionary, &fields, &total)) {
         lua_pushlightuserdata(Lua, fields);
         lua_pushinteger(Lua, total);
         lua_pushcclosure(Lua, object_next_ipair, 2);
         return 1;
      }
      else luaL_error(Lua, "Object class defines no fields.");
   }
   else luaL_error(Lua, "Expected object.");
   return 0;
}

//********************************************************************************************************************

#include "fluid_objects_indexes.cpp"
#include "fluid_objects_calls.cpp"

//********************************************************************************************************************
// Register the object interface.

static const luaL_Reg objectlib_functions[] = {
   { "new",  object_new },
   { "find", object_find },
   { "class", object_class },
   { NULL, NULL}
};

static const luaL_Reg objectlib_methods[] = {
   { "__index",    object_index },
   { "__newindex", object_newindex },
   { "__tostring", object_tostring },
   { "__gc",       object_destruct },
   { "__pairs",    object_pairs },
   { "__ipairs",   object_ipairs },
   { NULL, NULL }
};

void register_object_class(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   log.trace("Registering object interface.");

   luaL_newmetatable(Lua, "Fluid.obj");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, NULL, objectlib_methods, 0);
   luaL_openlib(Lua, "obj", objectlib_functions, 0);

   OJH_init        = simple_hash("init");
   OJH_free        = simple_hash("free");
   OJH_lock        = simple_hash("lock");
   OJH_children    = simple_hash("children");
   OJH_detach      = simple_hash("detach");
   OJH_get         = simple_hash("get");
   OJH_new         = simple_hash("new");
   OJH_state       = simple_hash("state");
   OJH_var         = simple_hash("var");
   OJH_getVar      = simple_hash("getVar");
   OJH_set         = simple_hash("set");
   OJH_setVar      = simple_hash("setVar");
   OJH_delayCall   = simple_hash("delayCall");
   OJH_exists      = simple_hash("exists");
   OJH_subscribe   = simple_hash("subscribe");
   OJH_unsubscribe = simple_hash("unsubscribe");
}
