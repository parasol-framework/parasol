/*

Some notes about object ownership and the safe handling of them:

The core's technical design means that any object that is not *directly* owned by the Lua Script must be treated as
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
#include <parasol/strings.hpp>
#include <inttypes.h>
#include <string_view>
#include <ranges>
#include <algorithm>

#include "lua.h"
#include "lib.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lj_obj.h"
#include "lj_object.h"
#include "hashes.h"
#include "defs.h"

template<class... Args> void RMSG(Args...) {
   //log.trace(Args)  // Enable if you want to debug results returned from functions, actions etc
}

static constexpr uint32_t OJH_init      = simple_hash("init");
static constexpr uint32_t OJH_free      = simple_hash("free");
static constexpr uint32_t OJH_lock      = simple_hash("lock");
static constexpr uint32_t OJH_children  = simple_hash("children");
static constexpr uint32_t OJH_detach    = simple_hash("detach");
static constexpr uint32_t OJH_get       = simple_hash("get");
static constexpr uint32_t OJH_new       = simple_hash("new");
static constexpr uint32_t OJH_state     = simple_hash("_state");
static constexpr uint32_t OJH_state_dep = simple_hash("state"); // Deprecated, use _state
static constexpr uint32_t OJH_getKey    = simple_hash("getKey");
static constexpr uint32_t OJH_set       = simple_hash("set");
static constexpr uint32_t OJH_setKey    = simple_hash("setKey");
static constexpr uint32_t OJH_delayCall = simple_hash("delayCall");
static constexpr uint32_t OJH_exists    = simple_hash("exists");
static constexpr uint32_t OJH_subscribe = simple_hash("subscribe");
static constexpr uint32_t OJH_unsubscribe = simple_hash("unsubscribe");

[[nodiscard]] static int object_action_call_args(lua_State *);
[[nodiscard]] static int object_method_call_args(lua_State *);
[[nodiscard]] static int object_action_call(lua_State *);
[[nodiscard]] static int object_method_call(lua_State *);
[[nodiscard]] static int get_results(lua_State *, const FunctionField *, const int8_t *);
[[nodiscard]] static ERR set_object_field(lua_State *, OBJECTPTR, CSTRING, int);

[[nodiscard]] static int object_children(lua_State *);
[[nodiscard]] static int object_detach(lua_State *);
[[nodiscard]] static int object_exists(lua_State *);
[[nodiscard]] static int object_free(lua_State *);
[[nodiscard]] static int object_get(lua_State *);
[[nodiscard]] static int object_getkey(lua_State *);
[[nodiscard]] static int object_init(lua_State *);
[[nodiscard]] static int object_lock(lua_State *);
[[nodiscard]] static int object_newchild(lua_State *);
[[nodiscard]] int object_newindex(lua_State *);
[[nodiscard]] static int object_set(lua_State *);
[[nodiscard]] static int object_setkey(lua_State *);
[[nodiscard]] static int object_state(lua_State *);
[[nodiscard]] static int object_state_dep(lua_State *);
[[nodiscard]] static int object_subscribe(lua_State *);
[[nodiscard]] static int object_unsubscribe(lua_State *);

[[nodiscard]] static int object_get_id(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_rgb(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_array(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_struct(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_string(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_object(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_ptr(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_double(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_large(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_ulong(lua_State *, const obj_read &, GCobject *);
[[nodiscard]] static int object_get_long(lua_State *, const obj_read &, GCobject *);

[[nodiscard]] static ERR object_set_array(lua_State *, OBJECTPTR, Field *, int);
[[nodiscard]] static ERR object_set_function(lua_State *, OBJECTPTR, Field *, int);
[[nodiscard]] static ERR object_set_object(lua_State *, OBJECTPTR, Field *, int);
[[nodiscard]] static ERR object_set_ptr(lua_State *, OBJECTPTR, Field *, int);
[[nodiscard]] static ERR object_set_double(lua_State *, OBJECTPTR, Field *, int);
[[nodiscard]] static ERR object_set_lookup(lua_State *, OBJECTPTR, Field *, int);
[[nodiscard]] static ERR object_set_oid(lua_State *, OBJECTPTR, Field *, int);
[[nodiscard]] static ERR object_set_number(lua_State *, OBJECTPTR, Field *, int);

//********************************************************************************************************************

#include "fluid_object_actions.cpp"

// Per-class field table cache - defined here, declared extern in defs.h
std::unordered_map<objMetaClass *, READ_TABLE> glClassReadTable;
std::unordered_map<objMetaClass *, WRITE_TABLE> glClassWriteTable;

inline void SET_CONTEXT(lua_State *Lua, APTR Function) {
   lua_pushvalue(Lua, 1); // Duplicate the object reference
   lua_pushcclosure(Lua, (lua_CFunction)Function, 1); // C function to call, +1 value for the object reference
}

[[nodiscard]] static int stack_object_children(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_children); return 1; }
[[nodiscard]] static int stack_object_detach(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_detach); return 1; }
[[nodiscard]] static int stack_object_exists(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_exists); return 1; }
[[nodiscard]] static int stack_object_free(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_free); return 1; }
[[nodiscard]] static int stack_object_get(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_get); return 1; }
[[nodiscard]] static int stack_object_getKey(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_getkey); return 1; }
[[nodiscard]] static int stack_object_init(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_init); return 1; }
[[nodiscard]] static int stack_object_lock(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_lock); return 1; }
[[nodiscard]] static int stack_object_newchild(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_newchild); return 1; }
[[nodiscard]] static int stack_object_set(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_set); return 1; }
[[nodiscard]] static int stack_object_setKey(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_setkey); return 1; }
[[nodiscard]] static int stack_object_state(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_state); return 1; }
[[nodiscard]] static int stack_object_state_dep(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_state_dep); return 1; }
[[nodiscard]] static int stack_object_subscribe(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_subscribe); return 1; }
[[nodiscard]] static int stack_object_unsubscribe(lua_State *Lua, const obj_read &Handle, GCobject *def) { SET_CONTEXT(Lua, (APTR)object_unsubscribe); return 1; }

//********************************************************************************************************************
// Hash designed to handle cases like `UID` -> `uid` and `RGBValue` -> `rgbValue`

[[nodiscard]] static uint32_t field_hash(std::string_view Name) {
   uint32_t hash = 5381;
   size_t k = 0;
   while ((k < Name.size()) and std::isupper(Name[k])) {
      hash = char_hash(std::tolower(Name[k]), hash);
      if (++k >= Name.size()) break;
      if ((k + 1 >= Name.size()) or (std::isupper(Name[k+1]))) continue;
      else break;
   }
   while (k < Name.size()) {
      hash = char_hash(Name[k], hash);
      k++;
   }
   return hash;
}

//********************************************************************************************************************

[[nodiscard]] static int obj_jump_method(lua_State *Lua, const obj_read &Handle, GCobject *def)
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

// Get the read table for a class, creating it if not present.
// The table is cached per-class in glClassReadTable.
READ_TABLE * get_read_table(lua_State *L, objMetaClass *Class)
{
   // Check if table already exists for this class
   if (auto it = glClassReadTable.find(Class); it != glClassReadTable.end()) {
      return &it->second;
   }

   READ_TABLE jmp;

   // Every possible action is hashed because both sub-class and base-class actions require support.

   for (auto code : std::views::iota(1, int(AC::END))) {
      auto hash = simple_hash(glActions[code].Name, simple_hash("ac"));
      jmp.insert(obj_read(hash, glJumpActions[code]));
   }

   MethodEntry *methods;
   int total_methods;
   if (Class->get(FID_Methods, methods, total_methods) IS ERR::Okay) {
      auto methods_span = std::span(methods, total_methods);
      for (auto &method : methods_span | std::views::drop(1)) {
         if (method.MethodID != AC::NIL) {
            auto hash = simple_hash(method.Name, simple_hash("mt"));
            jmp.insert(obj_read(hash, obj_jump_method, &method));
         }
      }
   }

   Field *dict;
   int total_dict;
   if (Class->get(FID_Dictionary, dict, total_dict) IS ERR::Okay) {
      jmp.insert(obj_read(simple_hash("id"), object_get_id));

      auto dict_span = std::span(dict, total_dict);
      for (auto &field : dict_span | std::views::filter([](const auto& f) { return f.Flags & FDF_R; })) {
         auto hash = field_hash(field.Name);

         if (field.Flags & FD_ARRAY) {
            if (field.Flags & FD_RGB) jmp.insert(obj_read(hash, object_get_rgb, &field));
            else jmp.insert(obj_read(hash, object_get_array, &field));
         }
         else if (field.Flags & FD_STRUCT) jmp.insert(obj_read(hash, object_get_struct, &field));
         else if (field.Flags & FD_STRING) jmp.insert(obj_read(hash, object_get_string, &field));
         else if (field.Flags & FD_POINTER) {
            if (field.Flags & (FD_OBJECT|FD_LOCAL)) { // Writing to an integral is permitted if marked as writeable.
               jmp.insert(obj_read(hash, object_get_object, &field));
            }
            else jmp.insert(obj_read(hash, object_get_ptr, &field));
         }
         else if (field.Flags & FD_DOUBLE) jmp.insert(obj_read(hash, object_get_double, &field));
         else if (field.Flags & FD_INT64) jmp.insert(obj_read(hash, object_get_large, &field));
         else if (field.Flags & FD_INT) {
            if (field.Flags & FD_UNSIGNED) jmp.insert(obj_read(hash, object_get_ulong, &field));
            else jmp.insert(obj_read(hash, object_get_long, &field));
         }
         else if (field.Flags & FD_FUNCTION); // Unsupported
         else pf::Log().warning("Unable to support field %s.%s for reading", Class->Name, field.Name);
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
   jmp.emplace(OJH_state_dep, stack_object_state_dep);
   jmp.emplace(OJH_getKey, stack_object_getKey);
   jmp.emplace(OJH_set, stack_object_set);
   jmp.emplace(OJH_setKey, stack_object_setKey);
   jmp.emplace(OJH_exists, stack_object_exists);
   jmp.emplace(OJH_subscribe, stack_object_subscribe);
   jmp.emplace(OJH_unsubscribe, stack_object_unsubscribe);

   glClassReadTable[Class] = std::move(jmp);
   return &glClassReadTable[Class];
}

//********************************************************************************************************************
// Get the write table for a class, creating it if not present.  The table is cached per-class in glClassWriteTable.

WRITE_TABLE * get_write_table(objMetaClass *Class)
{
   // Check if table already exists for this class
   if (auto it = glClassWriteTable.find(Class); it != glClassWriteTable.end()) {
      return &it->second;
   }

   WRITE_TABLE jmp;
   Field *dict;
   int total_dict;
   if (Class->get(FID_Dictionary, dict, total_dict) IS ERR::Okay) {
      auto dict_span = std::span(dict, total_dict);
      for (auto& field : dict_span | std::views::filter([](const auto& f) { return f.Flags & (FD_W|FD_I); })) {
         char ch[2] = { field.Name[0], 0 };
         if ((ch[0] >= 'A') and (ch[0] <= 'Z')) ch[0] = ch[0] - 'A' + 'a';
         auto hash = simple_hash(field.Name+1, simple_hash(ch));

         if (field.Flags & FD_ARRAY) {
            jmp.insert(obj_write(hash, object_set_array, &field));
         }
         else if (field.Flags & FD_FUNCTION) {
            jmp.insert(obj_write(hash, object_set_function, &field));
         }
         else if (field.Flags & FD_POINTER) {
            if (field.Flags & (FD_OBJECT|FD_LOCAL)) {
               jmp.insert(obj_write(hash, object_set_object, &field));
            }
            else jmp.insert(obj_write(hash, object_set_ptr, &field));
         }
         else if (field.Flags & (FD_DOUBLE|FD_FLOAT)) {
            jmp.insert(obj_write(hash, object_set_double, &field));
         }
         else if (field.Flags & (FD_FLAGS|FD_LOOKUP)) {
            jmp.insert(obj_write(hash, object_set_lookup, &field));
         }
         else if (field.Flags & FD_OBJECT) { // Object ID
            jmp.insert(obj_write(hash, object_set_oid, &field));
         }
         else if (field.Flags & (FD_INT|FD_INT64)) {
            jmp.insert(obj_write(hash, object_set_number, &field));
         }
      }
   }

   glClassWriteTable[Class] = std::move(jmp);
   return &glClassWriteTable[Class];
}

//********************************************************************************************************************
// Any Read accesses to the object will pass through here.  The requested key must exist in the hashed jump-table for
// the targeted class, or an error will be returned.

[[nodiscard]] int object_index(lua_State *Lua)
{
   auto tv = Lua->base;
   if (tvisobject(tv)) {
      auto def = objectV(tv);
      auto keyname = luaL_checkstring(Lua, 2);

      if (!def->uid) { // Check if the object has been dereferenced by free() or similar
         luaL_error(Lua, ERR::DoesNotExist, "Object dereferenced, unable to read field %s", keyname);
         return 0;
      }

      auto read_table = get_read_table(Lua, def->classptr);
      auto hash_key = obj_read(simple_hash(keyname));
      if (auto func = read_table->find(hash_key); func != read_table->end()) {
         return func->Call(Lua, *func, def);
      }
      else {
         pf::Log(__FUNCTION__).warning("Field does not exist or is unreadable: %s.%s", def->classptr ? def->classptr->ClassName: "?", keyname);
         Lua->CaughtError = ERR::NoSupport;
      }
   }

   return 0;
}

//********************************************************************************************************************

[[nodiscard]] static ACTIONID get_action_info(lua_State *Lua, CLASSID ClassID, CSTRING action, const FunctionField **Args)
{
   pf::Log log;

   if ((action[0] IS 'm') and (action[1] IS 't')) { // User is explicitly referring to a method
      action += 2;
   }
   else {
      if (auto it = glActionLookup.find(action); it != glActionLookup.end()) {
         *Args = glActions[int(it->second)].Args;
         return it->second;
      }
   }

   *Args = nullptr;
   if (auto mc = FindClass(ClassID)) {
      MethodEntry *table;
      int total_methods;
      ACTIONID action_id;
      if ((mc->get(FID_Methods, table, total_methods) IS ERR::Okay) and (table)) {
         for (int i=1; i < total_methods; i++) {
            if ((table[i].Name) and (iequals(action, table[i].Name))) {
               action_id = table[i].MethodID;
               *Args = table[i].Args;
               i = 0x7ffffff0;
               return action_id;
            }
         }
      }
      else log.warning("No methods declared for class %s, cannot call %s()", mc->ClassName, action);
   }
   else luaL_error(Lua, ERR::Search);

   return AC::NIL;
}

/*********************************************************************************************************************
** Usage: object = obj.new("Display", { field1 = value1, field2 = value2, ...})
**
** If fields are provided in the second argument, the object will be initialised automatically.  If no field list is
** provided, object.init() must be used to initialise the object.
**
** Variable fields can be denoted with an underscore prefix.
**
** Also see object_newchild() for creating objects from a parent.
**
** Never returns nil, errors are immediately thrown.
*/

[[nodiscard]] static int object_new(lua_State *Lua)
{
   pf::Log log("obj.new");
   CSTRING class_name;
   CLASSID class_id;

   NF objflags = NF::NIL;
   int type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      class_id = CLASSID(lua_tointeger(Lua, 1));
      class_name = nullptr;
      log.trace("$%.8x", uint32_t(class_id));
   }
   else if ((class_name = luaL_checkstring(Lua, 1))) {
      class_id = CLASSID(strihash(class_name));
      log.trace("%s, $%.8x", class_name, uint32_t(class_id));
   }
   else {
      log.warning("String or ID expected for class name, got '%s'.", lua_typename(Lua, type));
      luaL_error(Lua, ERR::Mismatch);
      return 0;
   }

   OBJECTPTR obj;
   if (auto error = NewObject(class_id, objflags, &obj); error IS ERR::Okay) {
      if (Lua->script->TargetID) obj->set(FID_Owner, Lua->script->TargetID);

      obj->CreatorMeta = Lua;

      load_include_for_class(Lua, obj->Class);

      auto def = lua_pushobject(Lua, obj->UID, obj, obj->Class, 0);
      if (lua_istable(Lua, 2)) {
         // Set fields against the object and initialise the object.  NOTE: Lua's table management code *does not*
         // preserve the order in which the fields were originally passed to the table.

         ERR field_error    = ERR::Okay;
         CSTRING field_name = nullptr;
         int failed_type    = LUA_TNONE;
         lua_pushnil(Lua);  // Access first key for lua_next()
         while (lua_next(Lua, 2) != 0) {
            if ((field_name = luaL_checkstring(Lua, -2))) {
               if (iequals("owner", field_name)) field_error = ERR::UnsupportedOwner; // Setting the owner is not permitted.
               else field_error = set_object_field(Lua, obj, field_name, -1);
            }
            else field_error = ERR::UnsupportedField;

            if (field_error != ERR::Okay) { // Break the loop early on error.
               failed_type = lua_type(Lua, -1);
               lua_pop(Lua, 2);
               break;
            }
            else lua_pop(Lua, 1);  // removes 'value'; keeps 'key' for the proceeding lua_next() iteration
         }

         if ((field_error != ERR::Okay) or ((error = InitObject(obj)) != ERR::Okay)) {
            class_name = obj->className();
            FreeResource(obj);

            if (field_error != ERR::Okay) {
               luaL_error(Lua, field_error, "Failed to set field '%s.%s' with %s, error: %s", class_name, field_name, lua_typename(Lua, failed_type), GetErrorMsg(field_error));
            }
            else {
               log.warning("Failed to Init() %s: %s", class_name, GetErrorMsg(error));
               luaL_error(Lua, error);
            }
            return 0;
         }
      }

      return 1;
   }
   else {
      luaL_error(Lua, ERR::NewObject, "NewObject() failed for class '%s', error: %s", class_name, GetErrorMsg(error));
      return 0;
   }
}

//********************************************************************************************************************
// Usage: state = some_object._state()
//
// Returns a table that can be used to store information that is specific to the object.  The state is linked to the
// object ID to ensure that the state values are still accessible if referenced elsewhere in the script.

static int object_state(lua_State *Lua)
{
   auto def = object_context(Lua);
   auto prv = (prvFluid *)Lua->script->ChildPrivate;

   // TODO: At this time no cleanup is performed on prv->StateMap.  Ideally this would be done with a hook into garbage
   // collection cycles.

   pf::Log log(__FUNCTION__);
   if (auto it = prv->StateMap.find(def->uid); it != prv->StateMap.end()) {
      lua_rawgeti(Lua, LUA_REGISTRYINDEX, it->second);
      return 1;
   }
   else {
      lua_createtable(Lua, 0, 0); // Create a new table on the stack.
      auto state_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);
      prv->StateMap[def->uid] = state_ref;
      lua_rawgeti(Lua, LUA_REGISTRYINDEX, state_ref);
      return 1;
   }
}

static int object_state_dep(lua_State *Lua)
{
   pf::Log log;
   log.warning("state() is deprecated, use _state().");
   return object_state(Lua);
}

//********************************************************************************************************************
// Syntactic sugar for creating new objects against a parent, e.g. window.new("button", { ... }).  Behaviour is
// mostly identical to obj.new() but the object is detached.

static int object_newchild(lua_State *Lua)
{
   pf::Log log("obj.child");

   auto parent = object_context(Lua);

   CSTRING class_name;
   CLASSID class_id;
   NF objflags = NF::NIL;
   int type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      class_id = CLASSID(lua_tointeger(Lua, 1));
      class_name = nullptr;
      log.trace("$%.8x", class_id);
   }
   else if ((class_name = luaL_checkstring(Lua, 1))) {
      class_id = CLASSID(strihash(class_name));
      log.trace("%s, $%.8x", class_name, class_id);
   }
   else {
      log.warning("String or ID expected for class name, got '%s'.", lua_typename(Lua, type));
      luaL_error(Lua, ERR::Mismatch);
      return 0;
   }

   OBJECTPTR obj;
   ERR error;
   if ((error = NewObject(class_id, objflags, &obj)) IS ERR::Okay) {
      if (Lua->script->TargetID) obj->set(FID_Owner, Lua->script->TargetID);

      obj->CreatorMeta = Lua;

      load_include_for_class(Lua, obj->Class);

      // Create as detached since it's owned by parent, not the script
      lua_pushobject(Lua, obj->UID, nullptr, obj->Class, GCOBJ_DETACHED);

      lua_pushinteger(Lua, parent->uid); // ID of the would-be parent.

      if (set_object_field(Lua, obj, "owner", lua_gettop(Lua)) != ERR::Okay) {
         FreeResource(obj);
         luaL_error(Lua, ERR::SetField);
         return 0;
      }

      lua_pop(Lua, 1);

      if (lua_istable(Lua, 2)) {
         // Set fields against the object and initialise the object.  NOTE: Lua's table management code *does not*
         // preserve the order in which the fields were originally passed to the table.

         ERR field_error = ERR::Okay;
         CSTRING field_name = nullptr;
         lua_pushnil(Lua);  // Access first key for lua_next()
         while (lua_next(Lua, 2) != 0) {
            if ((field_name = luaL_checkstring(Lua, -2))) {
               if (iequals("owner", field_name)) field_error = ERR::UnsupportedOwner; // Setting the owner is not permitted.
               else field_error = set_object_field(Lua, obj, field_name, -1);
            }
            else field_error = ERR::UnsupportedField;

            if (field_error != ERR::Okay) { // Break the loop early on error.
               lua_pop(Lua, 2);
               break;
            }
            else lua_pop(Lua, 1);  // removes 'value'; keeps 'key' for the proceeding lua_next() iteration
         }

         if ((field_error != ERR::Okay) or ((error = InitObject(obj)) != ERR::Okay)) {
            FreeResource(obj);

            if (field_error != ERR::Okay) {
               luaL_error(Lua, field_error, "Failed to set field '%s', error: %s", field_name, GetErrorMsg(field_error));
            }
            else {
               log.warning("Failed to Init() object '%s', error: %s", class_name, GetErrorMsg(error));
               luaL_error(Lua, ERR::Init);
            }
            return 0;
         }
      }

      return 1;
   }
   else {
      luaL_error(Lua, ERR::NewObject);
      return 0;
   }
}

//********************************************************************************************************************
// Guaranteed to not throw exceptions.

ERR push_object_id(lua_State *Lua, OBJECTID ObjectID)
{
   if (!ObjectID) { lua_pushnil(Lua); return ERR::Okay; }

   if (auto object = GetObjectPtr(ObjectID)) {
      lua_pushobject(Lua, ObjectID, nullptr, object->Class, GCOBJ_DETACHED);
      return ERR::Okay;
   }
   else {
      // Object doesn't exist, but we still push a reference to it
      lua_pushobject(Lua, ObjectID, nullptr, nullptr, GCOBJ_DETACHED);
      return ERR::Okay;
   }
}

/*********************************************************************************************************************
** Usage: object = obj.find("ObjectName" | ObjectID, [ClassName | ClassID])
**
** Returns nil on error - does not throw exceptions.
**
** The fluid object itself can be found by using the name "self".
*/

[[nodiscard]] static int object_find_ptr(lua_State *Lua, OBJECTPTR obj)
{
   // Private objects discovered by obj.find() have to be treated as an external reference at all times
   // (access must controlled by access_object() and release_object() calls).

   load_include_for_class(Lua, obj->Class);
   lua_pushobject(Lua, obj->UID, nullptr, obj->Class, GCOBJ_DETACHED);
   return 1;
}

[[nodiscard]] static int object_find(lua_State *Lua)
{
   pf::Log log("object.find");
   CSTRING object_name;
   CLASSID class_id;
   OBJECTID object_id;

   int type = lua_type(Lua, 1);
   if ((type IS LUA_TSTRING) and ((object_name = lua_tostring(Lua, 1)))) {
      int class_type = lua_type(Lua, 2); // Optional
      if (class_type IS LUA_TNUMBER) class_id = CLASSID(lua_tointeger(Lua, 2));
      else if (class_type IS LUA_TSTRING) class_id = CLASSID(strihash(lua_tostring(Lua, 2)));
      else class_id = CLASSID::NIL;

      log.trace("obj.find(%s, $%.8x)", object_name, class_id);

      if ((iequals("self", object_name)) and (class_id IS CLASSID::NIL)) {
         return object_find_ptr(Lua, Lua->script);
      }
      else if (iequals("owner", object_name)) {
         if (auto obj = Lua->script->Owner) return object_find_ptr(Lua, obj);
         else return 0;
      }

      if (FindObject(object_name, class_id, FOF::SMART_NAMES, &object_id) IS ERR::Okay) {
         return object_find_ptr(Lua, GetObjectPtr(object_id));
      }
      else log.detail("Unable to find object '%s'", object_name);
   }
   else if ((type IS LUA_TNUMBER) and ((object_id = lua_tointeger(Lua, 1)))) {
      log.trace("obj.find(#%d)", object_id);

      pf::ScopedObjectLock lock(object_id);
      if (lock.granted()) {
         return object_find_ptr(Lua, *lock);
      }
   }
   else log.warning("String or ID expected for object name, got '%s'.", lua_typename(Lua, type));

   return 0;
}

//********************************************************************************************************************
// Usage: metaclass = obj.class(object)
//
// Returns the MetaClass for an object, representing it as an inspectable object.

[[nodiscard]] static int object_class(lua_State *Lua)
{
   auto def = objectV(Lua->base);
   objMetaClass *cl = def->classptr;
   lua_pushobject(Lua, cl->UID, (OBJECTPTR)cl, cl, GCOBJ_DETACHED);
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

   GCobject *def = object_context(Lua);

   CLASSID class_id;
   CSTRING classfilter;
   if ((classfilter = luaL_optstring(Lua, 1, nullptr)) and (classfilter[0])) {
      class_id = CLASSID(strihash(classfilter));
   }
   else class_id = CLASSID::NIL;

   pf::vector<ChildEntry> list;
   if (ListChildren(def->uid, &list) IS ERR::Okay) {
      int index = 0;
      auto id = std::make_unique<int[]>(list.size());
      for (auto &rec : list) {
         if (class_id != CLASSID::NIL) {
            if (rec.ClassID IS class_id) id[index++] = rec.ObjectID;
         }
         else id[index++] = rec.ObjectID;
      }

      make_array(Lua, AET::INT32, index, id.get());
   }
   else make_array(Lua, AET::INT32);

   return 1; // make_array() always returns a value even if it is nil
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
   auto def = object_context(Lua);

   if (!lua_isfunction(Lua, 1)) {
      luaL_argerror(Lua, 1, "Function expected.");
      return 0;
   }

   if (access_object(def)) {
      pf::Log log("obj.lock");
      log.branch("Object: %d", def->uid);
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
   auto def = object_context(Lua);

   pf::Log log("obj.detach");
   log.traceBranch("Detached: %d", def->is_detached());

   if (!def->is_detached()) def->set_detached(true);

   return 0;
}

/*********************************************************************************************************************
** Usage: obj.exists()
**
** Returns true if the object still exists, otherwise nil.
*/

static int object_exists(lua_State *Lua)
{
   auto def = object_context(Lua);
   if (access_object(def)) {
      release_object(def);
      lua_pushboolean(Lua, true);
      return 1;
   }
   return 0;
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
   auto def = object_context(Lua);

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
   ACTIONID action_id = get_action_info(Lua, def->classptr->ClassID, action, &arglist);

   if (action_id IS AC::NIL) {
      luaL_argerror(Lua, 1, "Action/Method name is invalid.");
      return 0;
   }

   OBJECTPTR obj;
   if (!(obj = access_object(def))) {
      luaL_error(Lua, ERR::AccessObject);
      return 0;
   }

   pf::Log log("obj.subscribe");
   log.trace("Object: %d, Action: %s (ID %d)", def->uid, action, action_id);

   auto callback = C_FUNCTION(notify_action);
   callback.Context = Lua->script;
   if (auto error = SubscribeAction(obj, action_id, &callback); error IS ERR::Okay) {
      auto prv = (prvFluid *)Lua->script->ChildPrivate;
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
      acsub.ObjectID = def->uid;
      acsub.ActionID = action_id;

      release_object(def);
   }
   else {
      release_object(def);
      luaL_error(Lua, error);
   }
   return 0;
}

//********************************************************************************************************************
// Usage: obj.unsubscribe(ActionName)

static int object_unsubscribe(lua_State *Lua)
{
   pf::Log log("unsubscribe");

   auto def = object_context(Lua);

   CSTRING action;
   if (!(action = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Action name expected.");
      return 0;
   }

   const FunctionField *arglist;
   ACTIONID action_id = get_action_info(Lua, def->classptr->ClassID, action, &arglist);

   if (action_id IS AC::NIL) {
      luaL_argerror(Lua, 1, "Action/Method name is invalid.");
      return 0;
   }

   log.trace("Object: %d, Action: %s", def->uid, action);

   auto prv = (prvFluid *)Lua->script->ChildPrivate;
   std::erase_if(prv->ActionList, [&](auto& item) {
      bool should_remove = (item.ObjectID IS def->uid) and ((action_id IS AC::NIL) or (item.ActionID IS action_id));
      if (should_remove) {
         luaL_unref(Lua, LUA_REGISTRYINDEX, item.Function);
         if (item.Reference) luaL_unref(Lua, LUA_REGISTRYINDEX, item.Reference);
      }
      return should_remove;
   });

   return 0;
}

//********************************************************************************************************************
// Object garbage collector.
//
// NOTE: It is possible for the referenced object to have already been destroyed if it is owned by something outside of
// Fluid's environment.  This is commonplace for UI objects.  In addition the object's class may have been removed if
// the termination process is running during an expunge.

[[nodiscard]] static int object_destruct(lua_State *Lua)
{
   auto def = objectV(Lua->base);

   while (def->accesscount > 0) release_object(def);

   if (not def->is_detached()) {
      // Note that if the object's owner has switched to something out of our context, we
      // don't terminate it (an exception is applied for Recordset objects as these must be
      // owned by a Database object).

      if (auto obj = GetObjectPtr(def->uid)) {
         if ((obj->Class->BaseClassID IS CLASSID::RECORDSET) or (obj->Owner IS Lua->script) or (obj->ownerID() IS Lua->script->TargetID)) {
            pf::Log log("obj.destruct");
            log.trace("Freeing Fluid-owned object #%d.", def->uid);
            FreeResource(obj); // We can't presume that the object pointer would be valid
         }
      }
   }

   return 0;
}

//********************************************************************************************************************

static int object_free(lua_State *Lua)
{
   auto def = object_context(Lua);

   FreeResource(def->uid);

   // Mark the object as freed but preserve GC header fields (nextgc, marked, gct)
   // so the GC can still properly traverse and free the GCobject structure.
   def->uid = 0;
   def->ptr = nullptr;
   def->classptr = nullptr;
   def->flags = GCOBJ_DETACHED;  // Prevent double-free in lj_object_free
   def->accesscount = 0;

   return 0;
}

//********************************************************************************************************************
// Does not throw in normal operation, the error code of the initialisation is returned.

static int object_init(lua_State *Lua)
{
   auto def = object_context(Lua);

   if (auto obj = access_object(def)) {
      lua_pushinteger(Lua, int(InitObject(obj)));
      release_object(def);
      return 1;
   }
   else {
      luaL_error(Lua, ERR::AccessObject);
      return 0;
   }
}

//********************************************************************************************************************
// Prints the object interface as the object ID, e.g. #-10513

[[nodiscard]] static int object_tostring(lua_State *Lua)
{
   auto def = objectV(Lua->base);
   lua_pushfstring(Lua, "#%d", def->uid);
   return 1;
}

//********************************************************************************************************************
// Support for pairs() allows the meta fields of the object to be iterated.  Note that in next_pair(), the object
// interface isn't used but could be pushed as an upvalue if needed.

[[nodiscard]] static int object_next_pair(lua_State *Lua)
{
   auto fields = (Field *)lua_touserdata(Lua, lua_upvalueindex(1));
   int field_total = lua_tointeger(Lua, lua_upvalueindex(2));
   int field_index = lua_tointeger(Lua, lua_upvalueindex(3));

   if ((field_index >= 0) and (field_index < field_total)) {
      lua_pushinteger(Lua, field_index + 1);
      lua_replace(Lua, lua_upvalueindex(3)); // Update the field counter

      lua_pushstring(Lua, fields[field_index].Name);
      lua_pushinteger(Lua, fields[field_index].Flags);
      return 2;
   }
   else return 0; // Terminates the iteration
}

[[nodiscard]] static int object_pairs(lua_State *Lua)
{
   auto def = objectV(Lua->base);

   Field *fields;
   int total;
   if (def->classptr->get(FID_Dictionary, fields, total) IS ERR::Okay) {
      lua_pushlightuserdata(Lua, fields);
      lua_pushinteger(Lua, total);
      lua_pushinteger(Lua, 0);
      lua_pushcclosure(Lua, object_next_pair, 3);
      return 1;
   }
   else luaL_error(Lua, ERR::FieldSearch, "Object class defines no fields.");
   return 0;
}

//********************************************************************************************************************
// Similar to pairs(), but returns each field index and its name.

[[nodiscard]] static int object_next_ipair(lua_State *Lua)
{
   auto fields = (Field *)lua_touserdata(Lua, lua_upvalueindex(1));
   int field_total = lua_tointeger(Lua, lua_upvalueindex(2));
   int field_index = lua_tointeger(Lua, 2); // Arg 2 is the previous index.  It's nil if this is the first iteration.

   if ((field_index >= 0) and (field_index < field_total)) {
      lua_pushinteger(Lua, field_index + 1);
      lua_pushstring(Lua, fields[field_index].Name);
      return 2;
   }
   else return 0; // Terminates the iteration
}

[[nodiscard]] static int object_ipairs(lua_State *Lua)
{
   auto def = objectV(Lua->base);

   Field *fields;
   int total;
   if (def->classptr->get(FID_Dictionary, fields, total) IS ERR::Okay) {
      lua_pushlightuserdata(Lua, fields);
      lua_pushinteger(Lua, total);
      lua_pushcclosure(Lua, object_next_ipair, 2);
      return 1;
   }
   else luaL_error(Lua, ERR::FieldSearch, "Object class defines no fields.");
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
   { nullptr, nullptr}
};

static const luaL_Reg objectlib_methods[] = {
   { "__index",    object_index },
   { "__newindex", object_newindex },
   { "__tostring", object_tostring },
   { "__gc",       object_destruct },
   { "__pairs",    object_pairs },
   { "__ipairs",   object_ipairs },
   { nullptr, nullptr }
};

void register_object_class(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   log.trace("Registering object interface.");

   luaL_newmetatable(Lua, "Fluid.obj");
   lua_pushstring(Lua, "Fluid.obj");
   lua_setfield(Lua, -2, "__name");    // metatable.__name = "Fluid.obj"
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, nullptr, objectlib_methods, 0);

   // Register as base metatable for LJ_TOBJECT type
   // This enables metamethod dispatch for native GCobject values
   lua_pushvalue(Lua, -1);  // Duplicate the metatable
   lua_setbasemetatable(Lua, LJ_TOBJECT);

   luaL_openlib(Lua, "obj", objectlib_functions, 0);
}
