/*

Some notes about object ownership and the safe handling of them:

The core's technical design means that any object that is *not directly owned by the Lua Script* must be treated as
external to that script.  External objects must be locked appropriately whenever they are used.  Locking
ensures that threads can interact with the object safely and that the object cannot be prematurely terminated.

Only objects created through the standard obj.new() interface are permanently locked.  Those referenced through
obj.find(), push_object(), or children created with some_object.new() are marked as detached.

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

#define RMSG(a...) //MSG(a) // Enable if you want to debug results returned from functions, actions etc

static int object_call(lua_State *);
static LONG get_results(lua_State *, const FunctionField *, const BYTE *);
static int getfield(lua_State *, struct object *, CSTRING);
static ERROR set_object_field(lua_State *, OBJECTPTR, CSTRING, LONG);

static int object_setvar(lua_State *Lua);
static int object_set(lua_State *Lua);
static int object_get(lua_State *Lua);
static int object_getvar(lua_State *Lua);
static int object_newindex(lua_State *Lua);

/*********************************************************************************************************************
** This macro is used to convert Lua calls.
**
** From: xml.acDataFeed(1,2,3)
** To:   object_call(xml,1,2,3)
*/

INLINE void SET_CONTEXT(lua_State *Lua, APTR Function)
{
   lua_pushvalue(Lua, 1); // Duplicate the object reference
   lua_pushcclosure(Lua, (lua_CFunction)Function, 1); // C function to call - the number indicates the number of values pushed onto the stack that are to be associated as private values relevant to the C function being called
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
** Usage: object = obj.new("Screen", { field1 = value1, field2 = value2, ...})
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
      class_id = lua_tonumber(Lua, 1);
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

      struct object *object = (struct object *)lua_newuserdata(Lua, sizeof(struct object));
      ClearMemory(object, sizeof(struct object));

      luaL_getmetatable(Lua, "Fluid.obj");
      lua_setmetatable(Lua, -2);
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
               prv->CaughtError = error;
               luaL_error(Lua, GetErrorMsg(error));
            }
            return 0;
         }
      }

      object->prvObject = obj;
      object->ObjectID = obj->UID;
      object->ClassID  = obj->SubID ? obj->SubID : obj->ClassID;
      object->Class    = FindClass(object->ClassID); //obj->Class;

      // In theory, objects created with obj.new() can be permanently locked because they belong to the
      // script.  This prevents them from being deleted prior to garbage collection and use of FreeResource() will not
      // subvert Fluid's reference based locks.  If necessary, a permanent release of the lock can be achieved with
      // a call to detach() at any time by the client program.

      object->AccessCount = 0;
      object->Locked      = FALSE;

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
   struct object *object;
   if (!(object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   // Note: At this time no cleanup is performed on the StateMap.  Ideally this would be done with a hook into garbage
   // collection cycles.

   pf::Log log(__FUNCTION__);
   auto it = prv->StateMap.find(object->ObjectID);
   if (it != prv->StateMap.end()) {
      lua_rawgeti(Lua, LUA_REGISTRYINDEX, it->second);
      return 1;
   }
   else {
      lua_createtable(Lua, 0, 0); // Create a new table on the stack.
      auto state_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);
      prv->StateMap[object->ObjectID] = state_ref;
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
   struct object *parent;

   if (!(parent = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   CSTRING class_name;
   CLASSID class_id;
   NF objflags = NF::NIL;
   LONG type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      class_id = lua_tonumber(Lua, 1);
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

      auto def = (struct object *)lua_newuserdata(Lua, sizeof(struct object));
      ClearMemory(def, sizeof(struct object));

      luaL_getmetatable(Lua, "Fluid.obj");
      lua_setmetatable(Lua, -2);

      lua_pushinteger(Lua, parent->ObjectID); // ID of the would-be parent.
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

      // Objects created as children are treated as detached.

      def->prvObject   = NULL;
      def->AccessCount = 0;
      def->Locked   = FALSE;
      def->Detached = TRUE;
      def->ObjectID = obj->UID;
      def->ClassID  = obj->SubID ? obj->SubID : obj->ClassID;
      def->Class    = FindClass(def->ClassID); //obj->Class;
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

struct object * push_object(lua_State *Lua, OBJECTPTR Object)
{
   struct object *newobject;
   if ((newobject = (struct object *)lua_newuserdata(Lua, sizeof(struct object)))) {
      ClearMemory(newobject, sizeof(struct object));

      auto_load_include(Lua, Object->Class);

      newobject->prvObject   = NULL;
      newobject->ObjectID    = Object->UID;
      newobject->ClassID     = Object->SubID ? Object->SubID : Object->ClassID;
      newobject->Class       = FindClass(newobject->ClassID); //object->Class;
      newobject->Detached    = TRUE; // The object is not linked to this Lua value (i.e. do not free or garbage collect it).
      newobject->Locked      = FALSE;
      newobject->AccessCount = 0;

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

   struct object *newobject;
   if ((newobject = (struct object *)lua_newuserdata(Lua, sizeof(struct object)))) {
      ClearMemory(newobject, sizeof(struct object));

      newobject->prvObject = NULL;
      newobject->ObjectID  = ObjectID;
      newobject->ClassID   = GetClassID(ObjectID);
      newobject->Class     = FindClass(newobject->ClassID);
      newobject->Detached  = TRUE;
      newobject->Locked    = FALSE;
      newobject->AccessCount = 0;

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

   struct object *object = (struct object *)lua_newuserdata(Lua, sizeof(struct object)); // +1 stack
   ClearMemory(object, sizeof(struct object));
   luaL_getmetatable(Lua, "Fluid.obj"); // +1 stack
   lua_setmetatable(Lua, -2); // -1 stack

   object->prvObject   = NULL;
   object->ObjectID    = obj->UID;
   object->ClassID     = obj->SubID ? obj->SubID : obj->ClassID;
   object->Class       = FindClass(object->ClassID); //obj->Class;
   object->Detached    = TRUE;
   object->Locked      = FALSE;
   object->AccessCount = 0;
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
         class_id = StrHash(lua_tostring(Lua, 2), FALSE);
      }
      else class_id = 0;

      log.trace("obj.find(%s, $%.8x)", object_name, class_id);

      if ((!StrMatch("self", object_name)) and (!class_id)) {
         return object_find_ptr(Lua, Lua->Script);
      }
      else if (!StrMatch("owner", object_name)) {
         if ((obj = GetObjectPtr(Lua->Script->ownerID()))) {
            return object_find_ptr(Lua, obj);
         }
         else return 0;
      }

      if (!FindObject(object_name, class_id, FOF_SMART_NAMES, &object_id)) {
         return object_find_ptr(Lua, GetObjectPtr(object_id));
      }
      else log.debug("Unable to find object '%s'", object_name);
   }
   else if ((type IS LUA_TNUMBER) and ((object_id = lua_tointeger(Lua, 1)))) {
      log.trace("obj.find(#%d)", object_id);

      if (CheckObjectExists(object_id) != ERR_Okay) return 0;

      if ((obj = GetObjectPtr(Lua->Script->ownerID()))) {
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
   struct object *query;
   if (!(query = (struct object *)get_meta(Lua, 1, "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   objMetaClass *cl = query->Class;
   auto def = (struct object *)lua_newuserdata(Lua, sizeof(struct object)); // +1 stack
   ClearMemory(def, sizeof(struct object));
   luaL_getmetatable(Lua, "Fluid.obj"); // +1 stack
   lua_setmetatable(Lua, -2); // -1 stack

   def->prvObject   = cl;
   def->ObjectID    = cl->UID;
   def->ClassID     = cl->SubID ? cl->SubID : cl->ClassID;
   def->Class       = cl;
   def->Detached    = TRUE;
   def->Locked      = FALSE;
   def->AccessCount = 0;
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

   struct object *object;
   if (!(object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
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
   if (!ListChildren(object->ObjectID, &list)) {
      LONG index = 0;
      LONG id[list.size()];
      for (auto &rec : list) {
         if (class_id) {
            if (rec.ClassID IS class_id) id[index++] = rec.ObjectID;
         }
         else id[index++] = rec.ObjectID;
      }

      make_table(Lua, FD_LONG, index, &id);
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
   struct object *def;

   if (!(def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   if (!lua_isfunction(Lua, 1)) {
      luaL_argerror(Lua, 1, "Function expected.");
      return 0;
   }

   if (access_object(def)) {
      pf::Log log("obj.lock");
      log.branch("Object: %d", def->ObjectID);
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
   struct object *def;

   if (!(def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   pf::Log log("obj.detach");
   log.traceBranch("Detached: %d", def->Detached);

   if (!def->Detached) def->Detached = TRUE;

   return 0;
}

/*********************************************************************************************************************
** Usage: obj.exists()
**
** Returns true if the object still exists, otherwise nil.
*/

static int object_exists(lua_State *Lua)
{
   if (auto def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj")) {
      if (access_object(def)) {
         release_object(def);
         lua_pushboolean(Lua, TRUE);
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
   struct object *def;

   if (!(def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
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
   ACTIONID action_id = get_action_info(Lua, def->ClassID, action, &arglist);

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
   log.trace("Object: %d, Action: %s (ID %d)", def->ObjectID, action, action_id);

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   ERROR error;
   auto callback = make_function_stdc(notify_action, Lua->Script);
   if (!(error = SubscribeAction(obj, action_id, &callback))) {
      actionmonitor *acsub;
      if (!AllocMemory(sizeof(actionmonitor), MEM_DATA, &acsub)) {
         if (!lua_isnil(Lua, 3)) { // A custom reference for the callback can be specified in arg 3.
            lua_settop(prv->Lua, 3);
            acsub->Reference = luaL_ref(prv->Lua, LUA_REGISTRYINDEX); // Pops value from stack and returns it as a reference that can be used later.
         }

         lua_settop(prv->Lua, 2);
         acsub->Function = luaL_ref(prv->Lua, LUA_REGISTRYINDEX); // Pops value from stack and returns it as a reference that can be used later.
         acsub->Object   = def;
         acsub->Args     = arglist;
         acsub->ObjectID = def->ObjectID;
         acsub->ActionID = action_id;

         if (prv->ActionList) prv->ActionList->Prev = acsub;
         acsub->Next = prv->ActionList;
         prv->ActionList = acsub;

         release_object(def);
         return 0;
      }
      else {
         UnsubscribeAction(obj, action_id);
         release_object(def);
         luaL_error(Lua, GetErrorMsg(ERR_AllocMemory));
         return 0;
      }
   }
   else {
      release_object(def);
      luaL_error(Lua, GetErrorMsg(error));
      return 0;
   }

   release_object(def);
   return 0;
}

//********************************************************************************************************************
// Usage: obj.unsubscribe(ActionName)

static int object_unsubscribe(lua_State *Lua)
{
   pf::Log log("unsubscribe");

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   struct object *def;
   if (!(def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_error(Lua, "Expected object.");
      return 0;
   }

   CSTRING action;
   if (!(action = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Action name expected.");
      return 0;
   }

   const FunctionField *arglist;
   ACTIONID action_id = get_action_info(Lua, def->ClassID, action, &arglist);

   if (!action_id) {
      luaL_argerror(Lua, 1, "Action/Method name is invalid.");
      return 0;
   }

   log.trace("Object: %d, Action: %s", def->ObjectID, action);

   OBJECTPTR obj;
   if (!(obj = access_object(def))) {
      luaL_error(Lua, GetErrorMsg(ERR_AccessObject));
      return 0;
   }

   for (auto acsub=prv->ActionList, next=acsub; acsub; acsub = next) {
      next = acsub->Next;
      if (acsub->ObjectID IS def->ObjectID) {
         if ((!action_id) or (acsub->ActionID IS action_id)) {
            luaL_unref(Lua, LUA_REGISTRYINDEX, acsub->Function);
            if (acsub->Reference) luaL_unref(Lua, LUA_REGISTRYINDEX, acsub->Reference);

            UnsubscribeAction(obj, action_id);

            if (acsub->Prev) acsub->Prev->Next = acsub->Next;
            if (acsub->Next) acsub->Next->Prev = acsub->Prev;
            if (acsub IS prv->ActionList) prv->ActionList = acsub->Next;

            FreeResource(acsub);
            // Do not break (in case of multiple subscriptions)
         }
      }
   }

   release_object(def);

   lua_pushinteger(Lua, ERR_Okay);
   return 1;
}

//********************************************************************************************************************
// Usage: obj.delayCall()
//
// Delays the next action or method call that is taken against this object.

static int object_delaycall(lua_State *Lua)
{
   if (auto def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj")) def->DelayCall = TRUE;
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
   if (auto def = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      pf::Log log("obj.destruct");

      //log.traceBranch("obj.destruct(#%d, Owner #%d, Class $%.8x, Detached: %d, Locks: %d)", def->ObjectID, GetOwnerID(def->ObjectID), def->ClassID, def->Detached, def->AccessCount);

      while (def->AccessCount > 0) release_object(def);

      if (!def->Detached) {
         // Note that if the object's owner has switched to something out of our context, we
         // don't terminate it (an exception is applied for Recordset objects as these must be
         // owned by a Database object).

         auto owner_id = GetOwnerID(def->ObjectID);
         if ((def->ClassID IS ID_RECORDSET) or (owner_id IS Lua->Script->UID) or (owner_id IS Lua->Script->TargetID)) {
            log.trace("Freeing Fluid-owned object #%d.", def->ObjectID);
            FreeResource(def->ObjectID); // We can't presume that the object pointer would be valid
         }
      }
   }

   return 0;
}

//********************************************************************************************************************

static int object_free(lua_State *Lua)
{
   if (auto def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj")) {
      FreeResource(def->ObjectID);
      ClearMemory(def, sizeof(struct object)); // Mark the object as unusable
   }

   return 0;
}

//********************************************************************************************************************

static int object_init(lua_State *Lua)
{
   if (auto def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj")) {
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
   if (auto def = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      pf::Log log("obj.tostring");
      log.trace("#%d", def->ObjectID);
      lua_pushfstring(Lua, "#%d", def->ObjectID);
   }
   else lua_pushstring(Lua, "?");
   return 1;
}

//********************************************************************************************************************
// Any Read accesses to the object will pass through here.

static int object_index(lua_State *Lua)
{
   if (auto def = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      if (auto code = luaL_checkstring(Lua, 2)) {
         pf::Log log;
         log.trace("obj.index(#%d, %s)", def->ObjectID, code);

         if ((code[0] IS 'a') and (code[1] IS 'c') and (code[2] >= 'A') and (code[2] <= 'Z')) {
            if (auto it = glActionLookup.find(code + 2); it != glActionLookup.end()) {
               lua_pushvalue(Lua, 1); // Arg1: Duplicate the object reference
               lua_pushinteger(Lua, it->second); // Arg2: Action ID
               lua_pushcclosure(Lua, object_call, 2);
               return 1;
            }

            luaL_error(Lua, "Action '%s' not recognised.", code+2);
            return 0;
         }
         else if ((code[0] IS 'm') and (code[1] IS 't') and (code[2] >= 'A') and (code[2] <= 'Z')) {
            // Method

            auto cl = FindClass(def->ClassID); //object->Class;
            if (!cl) {
               luaL_error(Lua, "Failed to resolve class %d", def->ClassID);
               return 0;
            }

            MethodEntry *table;
            LONG total_methods;
            if ((!GetFieldArray(cl, FID_Methods, &table, &total_methods)) and (table)) {
               for (LONG i=1; i < total_methods; i++) { // TODO: Sorted hash IDs and a binary search would be best
                  if (!StrMatch(table[i].Name, code+2)) {
                     lua_pushvalue(Lua, 1); // Arg1: Duplicate the object reference
                     lua_pushinteger(Lua, table[i].MethodID); // Arg2: Method ID
                     lua_pushlightuserdata(Lua, table + i); // Arg3: Method lookup table
                     lua_pushcclosure(Lua, object_call, 3); // Push a c closure with 3 input values on the stack
                     return 1;
                  }
               }
               luaL_error(Lua, "Class %s does not support requested method %s()", cl->ClassName, code+2);
            }
            else luaL_error(Lua, "No methods defined by class %s, cannot call %s()", cl->ClassName, code+2);
         }
         else {
            switch (StrHash(code, 0)) {
               case HASH_INIT:        SET_CONTEXT(Lua, (APTR)object_init); return 1;
               case HASH_FREE:        SET_CONTEXT(Lua, (APTR)object_free); return 1;
               case HASH_LOCK:        SET_CONTEXT(Lua, (APTR)object_lock); return 1;
               case HASH_CHILDREN:    SET_CONTEXT(Lua, (APTR)object_children); return 1;
               case HASH_DETACH:      SET_CONTEXT(Lua, (APTR)object_detach); return 1;
               case HASH_GET:         SET_CONTEXT(Lua, (APTR)object_get); return 1;
               case HASH_NEW:         SET_CONTEXT(Lua, (APTR)object_newchild); return 1;
               case HASH_STATE:       SET_CONTEXT(Lua, (APTR)object_state); return 1;
               case HASH_VAR:
               case HASH_GETVAR:      SET_CONTEXT(Lua, (APTR)object_getvar); return 1;
               case HASH_SET:         SET_CONTEXT(Lua, (APTR)object_set); return 1;
               case HASH_SETVAR:      SET_CONTEXT(Lua, (APTR)object_setvar); return 1;
               case HASH_DELAYCALL:   SET_CONTEXT(Lua, (APTR)object_delaycall); return 1;
               case HASH_EXISTS:      SET_CONTEXT(Lua, (APTR)object_exists); return 1;
               case HASH_SUBSCRIBE:   SET_CONTEXT(Lua, (APTR)object_subscribe); return 1;
               case HASH_UNSUBSCRIBE: SET_CONTEXT(Lua, (APTR)object_unsubscribe); return 1;
               default: {
                  // Default to retrieving the field name.  It's a good solution given the aforementioned string checks,
                  // so long as there are no fields named 'access' or 'release' and the user doesn't write field names
                  // with odd caps.

                  auto prv = (prvFluid *)Lua->Script->ChildPrivate;
                  prv->CaughtError = getfield(Lua, def, code);
                  if (!prv->CaughtError) return 1;
                  //if (prv->ThrowErrors) luaL_error(Lua, GetErrorMsg);
               }
            }
         }
      }
   }

   return 0;
}

//********************************************************************************************************************
// Support for pairs() allows the meta fields of the object to be iterated.  Note that in next_pair(), the object
// interface isn't used but could be pushed as an upvalue if needed.

static int object_next_pair(lua_State *Lua)
{
   auto fields = (FieldArray *)lua_touserdata(Lua, lua_upvalueindex(1));
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
   if (auto def = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      FieldArray *fields;
      LONG total;
      if (!GetFieldArray(def->Class, FID_Fields, &fields, &total)) {
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
   auto fields = (FieldArray *)lua_touserdata(Lua, lua_upvalueindex(1));
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
   struct object *def;
   if ((def = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      FieldArray *fields;
      LONG total;
      if (!GetFieldArray(def->Class, FID_Fields, &fields, &total)) {
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

static const struct luaL_Reg objectlib_functions[] = {
   { "new",  object_new },
   { "find", object_find },
   { "class", object_class },
   { NULL, NULL}
};

static const struct luaL_Reg objectlib_methods[] = {
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
}
