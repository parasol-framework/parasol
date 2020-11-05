/*

Some notes about object ownership and the safe handling of them:

The core's technical design means that any object that is not directly owned by the Lua Script object is to be
treated as external to the script.  External objects must be locked appropriately whenever they are used.  Locking
ensures that threads can interact with the object safely and that the object cannot be prematurely terminated.

Only objects created through the standard obj.new() interface are permanently locked.  Those referenced through
obj.find(), push_object(), or children created with some_object.new() are marked as detached.

*/

static int object_call(lua_State *);
static ERROR build_args(lua_State *, const struct FunctionField *, LONG, APTR, LONG *);
static LONG get_results(lua_State *, const struct FunctionField *, CPTR);
static int getfield(lua_State *, struct object *, CSTRING);
static ERROR set_object_field(lua_State *, OBJECTPTR, CSTRING, LONG);

static int object_setvar(lua_State *Lua);
static int object_set(lua_State *Lua);
static int object_get(lua_State *Lua);
static int object_getvar(lua_State *Lua);
static int object_newindex(lua_State *Lua);

/*****************************************************************************
** This macro is used to convert Lua calls.
**
** From: xml.acDataFeed(1,2,3)
** To:   object_call(xml,1,2,3)
*/

INLINE void SET_CONTEXT(lua_State *Lua, APTR Function)
{
   lua_pushvalue(Lua, 1); // Duplicate the object reference
   lua_pushcclosure(Lua, Function, 1); // C function to call - the number indicates the number of values pushed onto the stack that are to be associated as private values relevant to the C function being called
}

//****************************************************************************

static LONG get_action_info(lua_State *Lua, LONG ClassID, CSTRING action, const struct FunctionField **Args)
{
   *Args = NULL;

   ACTIONID action_id;
   if ((action[0] IS 'm') AND (action[1] IS 't')) {
      action += 2; // User is explicitly referring to a method
      action_id = 0;
   }
   else {
      ACTIONID *ptr;
      if (!VarGet(glActionLookup, action, &ptr, NULL)) {
         action_id = ptr[0];
         *Args = glActions[action_id].Args;
      }
      else action_id = 0;
   }

   if (!action_id) { // Search methods
      objMetaClass *class;
      if (!(class = FindClass(ClassID))) {
         luaL_error(Lua, GetErrorMsg(ERR_Search));
         return 0;
      }

      struct MethodArray *table;
      LONG total_methods;
      if ((!GetFieldArray(class, FID_Methods, &table, &total_methods)) AND (table)) {
         LONG i;
         for (i=1; i < total_methods+1; i++) {
            if ((table[i].Name) AND (!StrMatch(action, table[i].Name))) {
               action_id = table[i].MethodID;
               *Args = table[i].Args;
               i = 0x7ffffff0;
               break;
            }
         }
      }
      else LogF("@","No methods declared for class %s, cannot call %s()", class->ClassName, action);
   }

   return action_id;
}

/*****************************************************************************
** Usage: object = obj.new("Screen", { field1 = value1, field2 = value2, ...})
**
** If fields are provided in the second argument, the object will be initialised automatically.  If no field list is
** provided, acInit() must be used to initialise the object.
**
** Variable fields can be denoted with an underscore prefix.
**
** An object can be allocated as public by prefixing a '@' to the class name.
*/

static int object_new(lua_State *Lua)
{
   CSTRING class_name;
   CLASSID class_id;

   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   LONG objflags = 0;
   LONG type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      class_id = lua_tonumber(Lua, 1);
      class_name = NULL;
      MSG("obj.new(%d)", class_id);
   }
   else if ((class_name = luaL_checkstring(Lua, 1))) {
      if (class_name[0] IS '@') {
         class_name++;
         objflags |= NF_PUBLIC;
      }
      class_id = StrHash(class_name, 0);
      MSG("obj.new(%s,$%.8x)", class_name, class_id);
   }
   else {
      LogF("@obj.new","String or ID expected for class name, got '%s'.", lua_typename(Lua, type));
      prv->CaughtError = ERR_Mismatch;
      luaL_error(Lua, GetErrorMsg(ERR_Mismatch));
      return 0;
   }

   OBJECTPTR obj;
   OBJECTID obj_id;
   ERROR error;
   if (!(error = NewLockedObject(class_id, objflags, &obj, &obj_id))) {
      if (Lua->Script->TargetID) SetLong(obj, FID_Owner, Lua->Script->TargetID);

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

         if ((field_error) OR ((error = acInit(obj)) != ERR_Okay)) {
            acFree(obj);
            ReleaseObject(obj);

            if (field_error) {
               prv->CaughtError = field_error;
               luaL_error(Lua, "Failed to set field '%s', error: %s", field_name, GetErrorMsg(field_error));
            }
            else {
               LogF("@obj.new","Failed to Init() object '%s', error: %s", class_name, GetErrorMsg(error));
               prv->CaughtError = error;
               luaL_error(Lua, GetErrorMsg(error));
            }
            return 0;
         }
      }

      object->prvObject = obj;
      object->ObjectID = obj->UniqueID;
      object->ClassID  = obj->SubID ? obj->SubID : obj->ClassID;
      object->Class    = FindClass(object->ClassID); //obj->Class;
      if (obj->UniqueID < 0) {
         // If the object is shared, its address must be accessed through locking
         object->prvObject = NULL;
         object->AccessCount = 0;
         object->Locked = FALSE;
         ReleaseObject(obj);
      }
      else {
         // In theory, private objects created with obj.new() can be permanently locked because they belong to the
         // script.  This prevents them from being deleted prior to garbage collection and use of acFree() will not
         // subvert Fluid's reference based locks.  If necessary, a permanent release of the lock can be achieved with
         // a call to detach() at any time by the client program.

#define MAINTAIN_OBJECT_LOCK 1

#ifdef MAINTAIN_OBJECT_LOCK
         object->AccessCount = 1;
         object->Locked = TRUE;
         object->NewLock = TRUE;
#else
         object->prvObject = NULL;
         object->AccessCount = 0;
         object->Locked = FALSE;
         ReleaseObject(obj);
#endif
      }

      return 1;
   }
   else {
      prv->CaughtError = ERR_NewObject;
      luaL_error(Lua, "NewObject() failed for class '%s', error: %s", class_name, GetErrorMsg(ERR_NewObject));
      return 0;
   }
}

//****************************************************************************
// Syntactic sugar for creating new objects against a parent, e.g. window.new("button", { ... }).  Behaviour is
// mostly identical to obj.new() but the object is detached.

static int object_newchild(lua_State *Lua)
{
   struct object *parent;
   if (!(parent = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   CSTRING class_name;
   CLASSID class_id;
   LONG objflags = 0;
   LONG type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      class_id = lua_tonumber(Lua, 1);
      class_name = NULL;
      MSG("obj.child(%d)", class_id);
   }
   else if ((class_name = luaL_checkstring(Lua, 1))) {
      if (class_name[0] IS '@') {
         class_name++;
         objflags |= NF_PUBLIC;
      }
      class_id = StrHash(class_name, 0);
      MSG("obj.child(%s,$%.8x)", class_name, class_id);
   }
   else {
      LogF("@obj.child","String or ID expected for class name, got '%s'.", lua_typename(Lua, type));
      prv->CaughtError = ERR_Mismatch;
      luaL_error(Lua, GetErrorMsg(ERR_Mismatch));
      return 0;
   }

   OBJECTPTR obj;
   OBJECTID obj_id;
   ERROR error;
   if (!(error = NewLockedObject(class_id, objflags, &obj, &obj_id))) {
      if (Lua->Script->TargetID) SetLong(obj, FID_Owner, Lua->Script->TargetID);

      obj->CreatorMeta = Lua;

      auto_load_include(Lua, obj->Class);

      struct object *object = (struct object *)lua_newuserdata(Lua, sizeof(struct object));
      ClearMemory(object, sizeof(struct object));

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

         if ((field_error) OR ((error = acInit(obj)) != ERR_Okay)) {
            acFree(obj);
            ReleaseObject(obj);

            if (field_error) {
               prv->CaughtError = field_error;
               luaL_error(Lua, "Failed to set field '%s', error: %s", field_name, GetErrorMsg(field_error));
            }
            else {
               LogF("@obj.child","Failed to Init() object '%s', error: %s", class_name, GetErrorMsg(error));
               prv->CaughtError = ERR_Init;
               luaL_error(Lua, GetErrorMsg(ERR_Init));
            }
            return 0;
         }
      }

      // Objects created as children are treated as detached.

      object->prvObject   = NULL;
      object->AccessCount = 0;
      object->Locked   = FALSE;
      object->Detached = TRUE;
      object->ObjectID = obj->UniqueID;
      object->ClassID  = obj->SubID ? obj->SubID : obj->ClassID;
      object->Class    = FindClass(object->ClassID); //obj->Class;
      ReleaseObject(obj);
      return 1;
   }
   else {
      prv->CaughtError = ERR_NewObject;
      luaL_error(Lua, GetErrorMsg(ERR_NewObject));
      return 0;
   }
}

//****************************************************************************
// Throws exceptions.  Used for returning objects to the user.

static struct object * push_object(lua_State *Lua, OBJECTPTR Object)
{
   struct object *newobject;
   if ((newobject = (struct object *)lua_newuserdata(Lua, sizeof(struct object)))) {
      ClearMemory(newobject, sizeof(struct object));

      auto_load_include(Lua, Object->Class);

      newobject->prvObject   = NULL;
      newobject->ObjectID    = Object->UniqueID;
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

//****************************************************************************
// Guaranteed to not throw exceptions.

static ERROR push_object_id(lua_State *Lua, OBJECTID ObjectID)
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

/*****************************************************************************
** Usage: object = obj.find("ObjectName" | ObjectID, [ClassName | ClassID])
**
** Returns nil on error - does not throw exceptions.
**
** The fluid object itself can be found by using the name "self".
*/

static int object_find(lua_State *Lua)
{
   OBJECTPTR obj;
   CSTRING object_name;
   CLASSID class_id;
   OBJECTID object_id;

   LONG type = lua_type(Lua, 1);
   if ((type IS LUA_TSTRING) AND ((object_name = lua_tostring(Lua, 1)))) {
      LONG class_type = lua_type(Lua, 2); // Optional
      if (class_type IS LUA_TNUMBER) {
         class_id = lua_tointeger(Lua, 2);
      }
      else if (class_type IS LUA_TSTRING) {
         class_id = StrHash(lua_tostring(Lua, 2), FALSE);
      }
      else class_id = 0;

      MSG("obj.find(%s, $%.8x)", object_name, class_id);

      if ((!StrMatch("self", object_name)) AND (!class_id)) {
         obj = (OBJECTPTR)Lua->Script;
         goto private_object;
      }
      else if (!StrMatch("owner", object_name)) {
         if ((obj = GetObjectPtr(Lua->Script->Head.OwnerID))) {
            goto private_object;
         }
         else return 0;
      }

      if (!FindPrivateObject(object_name, &obj)) {
         // Private objects discovered by obj.find() have to be treated as an external reference at all times
         // (access must controlled by access_object() and release_object() calls).
private_object:
         auto_load_include(Lua, obj->Class);

         {
            struct object *object = (struct object *)lua_newuserdata(Lua, sizeof(struct object)); // +1 stack
            ClearMemory(object, sizeof(struct object));
            luaL_getmetatable(Lua, "Fluid.obj"); // +1 stack
            lua_setmetatable(Lua, -2); // -1 stack

            object->prvObject   = NULL;
            object->ObjectID    = obj->UniqueID;
            object->ClassID     = obj->SubID ? obj->SubID : obj->ClassID;
            object->Class       = FindClass(object->ClassID); //obj->Class;
            object->Detached    = TRUE;
            object->Locked      = FALSE;
            object->AccessCount = 0;
         }
         return 1;
      }
      else if (!FastFindObject(object_name, class_id, &object_id, 1, NULL)) {
public_object:
         {
            struct object *object = (struct object *)lua_newuserdata(Lua, sizeof(struct object));
            ClearMemory(object, sizeof(struct object));
            luaL_getmetatable(Lua, "Fluid.obj");
            lua_setmetatable(Lua, -2);

            object->prvObject   = NULL;
            object->ObjectID    = object_id;
            object->ClassID     = GetClassID(object_id);
            object->Class       = FindClass(object->ClassID);
            object->Detached    = TRUE;
            object->Locked      = FALSE;
            object->AccessCount = 0;
         }
         return 1;
      }
      else LogF("7obj.find","Unable to find object '%s'", object_name);
   }
   else if ((type IS LUA_TNUMBER) AND ((object_id = lua_tointeger(Lua, 1)))) {
      MSG("obj.find(#%d)", object_id);

      if (CheckObjectIDExists(object_id) != ERR_Okay) {
         return 0;
      }
      else if (object_id < 0) {
         goto public_object;
      }
      else {
         UBYTE buffer[32] = "#";
         IntToStr(object_id, buffer+1, sizeof(buffer)-1);
         if (!FindPrivateObject(buffer, &obj)) {
            goto private_object;
         }
      }
   }
   else LogF("@obj.find","String or ID expected for object name, got '%s'.", lua_typename(Lua, type));

   return 0;
}

//****************************************************************************
// Usage: metaclass = obj.class(object)
//
// Returns the meta class information for an object.

static int object_class(lua_State *Lua)
{
   struct object *query;
   if (!(query = get_meta(Lua, 1, "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   struct rkMetaClass *cl = query->Class;
   struct object *object = (struct object *)lua_newuserdata(Lua, sizeof(struct object)); // +1 stack
   ClearMemory(object, sizeof(struct object));
   luaL_getmetatable(Lua, "Fluid.obj"); // +1 stack
   lua_setmetatable(Lua, -2); // -1 stack

   object->prvObject = &cl->Head;
   object->ObjectID  = cl->Head.UniqueID;
   object->ClassID   = cl->Head.SubID ? cl->Head.SubID : cl->Head.ClassID;
   object->Class     = cl;
   object->Detached  = TRUE;
   object->Locked    = FALSE;
   object->AccessCount = 0;
   return 1;
}

//****************************************************************************
// Usage: obj.children(["ClassNameFilter"])
//
// Returns an object ID array of children belonging to the queried object.  If there are no children, an empty array is
// returned.

static int object_children(lua_State *Lua)
{
   MSG("obj.children()");

   struct object *object;
   if (!(object = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   CLASSID class_id;
   CSTRING classfilter;
   if ((classfilter = luaL_optstring(Lua, 1, NULL)) AND (classfilter[0])) {
      class_id = StrHash(classfilter, 0);
   }
   else class_id = 0;

   struct ChildEntry list[512];
   LONG id[512];
   LONG count = ARRAYSIZE(list);

   if (!ListChildren(object->ObjectID, list, &count)) {
      LONG index = 0;
      LONG i;
      for (i=0; i < count; i++) {
         if (class_id) {
            if (list[i].ClassID IS class_id) id[index++] = list[i].ObjectID;
         }
         else id[index++] = list[i].ObjectID;
      }

      make_table(Lua, FD_LONG, index, &id);
   }
   else make_table(Lua, FD_LONG, 0, NULL);

   return 1; // make_table() always returns a value even if it is nil
}

//****************************************************************************
// obj:lock(function()
//    --Code--
// end)
//
// This method will lock the target object and then execute the function.  The lock will be released on the function's
// completion.

static int object_lock(lua_State *Lua)
{
   struct object *object;

   if (!(object = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   if (object->ObjectID < 0) { // Security measure - public locks are not allowed.
      luaL_error(Lua, "Locking public objects is not supported.");
      return 0;
   }

   if (!lua_isfunction(Lua, 1)) {
      luaL_argerror(Lua, 1, "Function expected.");
      return 0;
   }

   if (access_object(object)) {
      LogF("~obj.lock()","Object: %d", object->ObjectID);
      lua_pcall(Lua, 0, 0, 0);
      LogReturn();
      release_object(object);
   }
   return 0;
}

//****************************************************************************
// Usage: obj:detach()
//
// Detaches the object from the metatable, this stops the object from being killed on garbage collection.  HOWEVER: The
// object will still belong to the Script, so once that is freed, the object will go down with it.

static int object_detach(lua_State *Lua)
{
   struct object *object;
   if (!(object = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   FMSG("~obj.detach()","Detached: %d, NewLock: %d", object->Detached, object->NewLock);

   if (!object->Detached) {
      object->Detached = TRUE;
      if (object->NewLock) { // If created by obj.new(), undo the persistent lock that we have.
         object->NewLock = FALSE;
         release_object(object);
      }
   }

   STEP();
   return 0;
}

/*****************************************************************************
** Usage: obj.exists()
**
** Returns true if the object still exists, otherwise nil.
*/

static int object_exists(lua_State *Lua)
{
   struct object *object;

   if ((object = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      OBJECTPTR obj;
      if ((obj = access_object(object))) {
         release_object(object);
         lua_pushboolean(Lua, TRUE);
         return 1;
      }
      else return 0;
   }
   else return 0;
}

/*****************************************************************************
** Usage: obj.subscribe(ActionName, Function, Reference)
**
** Subscribe a function to an action or method.  Throws an exception on failure.
*/

static int object_subscribe(lua_State *Lua)
{
   struct object *object;
   if (!(object = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_error(Lua, "Expected object.");
      return 0;
   }

   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   CSTRING action;
   if (!(action = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Action name expected.");
      return 0;
   }

   if (!lua_isfunction(Lua, 2)) {
      luaL_argerror(Lua, 2, "Function expected.");
      return 0;
   }

   const struct FunctionField *arglist;
   ACTIONID action_id = get_action_info(Lua, object->ClassID, action, &arglist);

   if (!action_id) {
      luaL_argerror(Lua, 1, "Action/Method name is invalid.");
      return 0;
   }

   FMSG("subscribe()","Object: %d, Action: %s (ID %d)", object->ObjectID, action, action_id);

   OBJECTPTR obj;
   if (!(obj = access_object(object))) {
      luaL_error(Lua, GetErrorMsg(ERR_AccessObject));
      return 0;
   }

   ERROR error;
   if (!(error = SubscribeActionTags(obj, action_id, TAGEND))) {
      struct actionmonitor *acsub;
      if (!AllocMemory(sizeof(struct actionmonitor), MEM_DATA, &acsub, NULL)) {
         if (!lua_isnil(Lua, 3)) { // A custom reference for the callback can be specified in arg 3.
            lua_settop(prv->Lua, 3);
            acsub->Reference = luaL_ref(prv->Lua, LUA_REGISTRYINDEX); // Pops value from stack and returns it as a reference that can be used later.
         }

         lua_settop(prv->Lua, 2);
         acsub->Function = luaL_ref(prv->Lua, LUA_REGISTRYINDEX); // Pops value from stack and returns it as a reference that can be used later.
         acsub->Object   = object;
         acsub->Args     = arglist;
         acsub->ObjectID = object->ObjectID;
         acsub->ActionID = action_id;

         if (prv->ActionList) prv->ActionList->Prev = acsub;
         acsub->Next = prv->ActionList;
         prv->ActionList = acsub;

         release_object(object);
         return 0;
      }
      else {
         UnsubscribeAction(obj, action_id);
         release_object(object);
         luaL_error(Lua, GetErrorMsg(ERR_AllocMemory));
         return 0;
      }
   }
   else {
      release_object(object);
      luaL_error(Lua, GetErrorMsg(error));
      return 0;
   }

   release_object(object);
   return 0;
}

//****************************************************************************
// Usage: obj.unsubscribe(ActionName)

static int object_unsubscribe(lua_State *Lua)
{
   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;

   struct object *object;
   if (!(object = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_error(Lua, "Expected object.");
      return 0;
   }

   CSTRING action;
   if (!(action = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Action name expected.");
      return 0;
   }

   const struct FunctionField *arglist;
   ACTIONID action_id = get_action_info(Lua, object->ClassID, action, &arglist);

   if (!action_id) {
      luaL_argerror(Lua, 1, "Action/Method name is invalid.");
      return 0;
   }

   FMSG("subscribe()","Object: %d, Action: %s", object->ObjectID, action);

   OBJECTPTR obj;
   if (!(obj = access_object(object))) {
      luaL_error(Lua, GetErrorMsg(ERR_AccessObject));
      return 0;
   }

   struct actionmonitor *acsub = prv->ActionList;
   while (acsub) {
      struct actionmonitor *next = acsub->Next;
      if (acsub->ObjectID IS object->ObjectID) {
         if ((!action_id) OR (acsub->ActionID IS action_id)) {
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
      acsub = next;
   }

   release_object(object);

   lua_pushinteger(Lua, ERR_Okay);
   return 1;
}

//****************************************************************************
// Usage: obj.delayCall()
//
// Delays the next action or method call that is taken against this object.

static int object_delaycall(lua_State *Lua)
{
   struct object *obj;
   if (!(obj = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   obj->DelayCall = TRUE;
   return 0;
}

//****************************************************************************
// Object garbage collector.

static int object_destruct(lua_State *Lua)
{
   struct object *object;
   if ((object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      #ifdef DEBUG
         struct rkMetaClass *cl = object->Class;
         if (cl) FMSG("~","obj.destruct(#%d, Owner #%d, Class %s, Detached: %d, Locks: %d)", object->ObjectID, GetOwnerID(object->ObjectID), cl->ClassName, object->Detached, object->AccessCount);
         else FMSG("~","obj.destruct(#%d, Owner #%d, Class $%.8x, Detached: %d, Locks: %d)", object->ObjectID, GetOwnerID(object->ObjectID), object->ClassID, object->Detached, object->AccessCount);
      #endif

      while (object->AccessCount > 0) {
         release_object(object);
      }

      if (!object->Detached) {
         // Object belongs to this Lua instance.  Note that it is possible that an object could destroy itself prior to
         // the garbage collector picking it up.  Because of this, we cannot rely on the integrity of the object
         // address and must free it on the ID.

         if (object->ObjectID > 0) {
            // Object is private.  Note that if the object's owner has switched to something out of our context, we
            // don't terminate it (an exception is applied for Recordset objects as these must be
            // owned by a Database object).

            OBJECTID owner_id = GetOwnerID(object->ObjectID);
            if ((object->ClassID IS ID_RECORDSET) OR (owner_id IS Lua->Script->Head.UniqueID) OR (owner_id IS Lua->Script->TargetID)) {
               MSG("Freeing Fluid-owned object #%d.", object->ObjectID);
               acFreeID(object->ObjectID);
            }
         }
         else {
            // Object is public
         }
      }
      STEP();
   }

   return 0;
}

//****************************************************************************
// Prints the object interface as the object ID, e.g. #-10513

static int object_tostring(lua_State *Lua)
{
   struct object *object;

   if ((object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      MSG("obj.tostring(%d)", object->ObjectID);
      lua_pushfstring(Lua, "#%d", object->ObjectID);
   }
   else lua_pushstring(Lua, "?");

   return 1;
}

//****************************************************************************
// Any Read accesses to the object will pass through here.

static int object_index(lua_State *Lua)
{
   struct object *object;

   if ((object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      CSTRING code;
      if ((code = luaL_checkstring(Lua, 2))) {
         MSG("obj.index(#%d, %s)", object->ObjectID, code);

         if ((code[0] IS 'a') AND (code[1] IS 'c') AND (code[2] >= 'A') AND (code[2] <= 'Z')) {
            ACTIONID *ptr;
            if (!VarGet(glActionLookup, code + 2, &ptr, NULL)) {
               lua_pushvalue(Lua, 1); // Arg1: Duplicate the object reference
               lua_pushinteger(Lua, ptr[0]); // Arg2: Action ID
               lua_pushcclosure(Lua, object_call, 2);
               return 1;
            }

            luaL_error(Lua, "Action '%s' not recognised.", code+2);
            return 0;
         }
         else if ((code[0] IS 'm') AND (code[1] IS 't') AND (code[2] >= 'A') AND (code[2] <= 'Z')) {
            // Method

            struct rkMetaClass *cl = FindClass(object->ClassID); //object->Class;
            if (!cl) {
               luaL_error(Lua, "Failed to resolve class %d", object->ClassID);
               return 0;
            }

            struct MethodArray *table;
            LONG total_methods, i;
            if ((!GetFieldArray(cl, FID_Methods, &table, &total_methods)) AND (table)) {
               for (i=1; i < total_methods+1; i++) { // TODO: Sorted hash IDs and a binary search would be best
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
               case HASH_LOCK:        SET_CONTEXT(Lua, object_lock); return 1;
               case HASH_CHILDREN:    SET_CONTEXT(Lua, object_children); return 1;
               case HASH_DETACH:      SET_CONTEXT(Lua, object_detach); return 1;
               case HASH_GET:         SET_CONTEXT(Lua, object_get); return 1;
               case HASH_NEW:         SET_CONTEXT(Lua, object_newchild); return 1;
               case HASH_VAR:
               case HASH_GETVAR:      SET_CONTEXT(Lua, object_getvar); return 1;
               case HASH_SET:         SET_CONTEXT(Lua, object_set); return 1;
               case HASH_SETVAR:      SET_CONTEXT(Lua, object_setvar); return 1;
               case HASH_DELAYCALL:   SET_CONTEXT(Lua, object_delaycall); return 1;
               case HASH_EXISTS:      SET_CONTEXT(Lua, object_exists); return 1;
               case HASH_SUBSCRIBE:   SET_CONTEXT(Lua, object_subscribe); return 1;
               case HASH_UNSUBSCRIBE: SET_CONTEXT(Lua, object_unsubscribe); return 1;
               default: {
                  // Default to retrieving the field name.  It's a good solution given the aforementioned string checks,
                  // so long as there are no fields named 'access' or 'release' and the user doesn't write field names
                  // with odd caps.

                  struct prvFluid *prv = Lua->Script->Head.ChildPrivate;
                  prv->CaughtError = getfield(Lua, object, code);
                  if (!prv->CaughtError) return 1;
                  //if (prv->ThrowErrors) luaL_error(Lua, GetErrorMsg);
               }
            }
         }
      }
   }

   return 0;
}

//****************************************************************************
// Support for pairs() allows the meta fields of the object to be iterated.  Note that in next_pair(), the object
// interface isn't used but could be pushed as an upvalue if needed.

static int object_next_pair(lua_State *Lua)
{
   struct FieldArray *fields = lua_touserdata(Lua, lua_upvalueindex(1));
   LONG field_total = lua_tointeger(Lua, lua_upvalueindex(2));
   LONG field_index = lua_tointeger(Lua, lua_upvalueindex(3));

   if ((field_index >= 0) AND (field_index < field_total)) {
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
   struct object *object;

   if ((object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      struct FieldArray *fields;
      LONG total;
      if (!GetFieldArray(object->Class, FID_Fields, &fields, &total)) {
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

//****************************************************************************
// Similar to pairs(), but returns each field index and its name.

static int object_next_ipair(lua_State *Lua)
{
   struct FieldArray *fields = lua_touserdata(Lua, lua_upvalueindex(1));
   LONG field_total = lua_tointeger(Lua, lua_upvalueindex(2));
   LONG field_index = lua_tointeger(Lua, 2); // Arg 2 is the previous index.  It's nil if this is the first iteration.

   if ((field_index >= 0) AND (field_index < field_total)) {
      lua_pushinteger(Lua, field_index + 1);
      lua_pushstring(Lua, fields[field_index].Name);
      return 2;
   }
   else return 0; // Terminates the iteration
}

static int object_ipairs(lua_State *Lua)
{
   struct object *object;

   if ((object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      struct FieldArray *fields;
      LONG total;
      if (!GetFieldArray(object->Class, FID_Fields, &fields, &total)) {
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

//****************************************************************************

#include "fluid_objects_indexes.c"
#include "fluid_objects_calls.c"

//****************************************************************************
// Register the object interface.

static const struct luaL_reg objectlib_functions[] = {
   { "new",  object_new },
   { "find", object_find },
   { "class", object_class },
   { NULL, NULL}
};

static const struct luaL_reg objectlib_methods[] = {
   { "__index",    object_index },
   { "__newindex", object_newindex },
   { "__tostring", object_tostring },
   { "__gc",       object_destruct },
   { "__pairs",    object_pairs },
   { "__ipairs",   object_ipairs },
   { NULL, NULL }
};

static void register_object_class(lua_State *Lua)
{
   MSG("Registering object interface.");

   luaL_newmetatable(Lua, "Fluid.obj");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, NULL, objectlib_methods, 0);
   luaL_openlib(Lua, "obj", objectlib_functions, 0);
}
