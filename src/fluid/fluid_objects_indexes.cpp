
//********************************************************************************************************************
// Usage: object.field = newvalue
//
// Custom fields can be referenced by using _ as a prefix.

static int object_newindex(lua_State *Lua)
{
   if (auto def = (object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      if (auto keyname = luaL_checkstring(Lua, 2)) {
         if (auto obj = access_object(def)) {
            ERR error;
            if (keyname[0] IS '_') error = acSetKey(obj, keyname+1, lua_tostring(Lua, 3));
            else {
               auto jt = get_write_table(def);
               if (auto func = jt->find(obj_write(simple_hash(keyname))); func != jt->end()) {
                  error = func->Call(Lua, obj, func->Field, 3);
               }
               else error = ERR::NoSupport;
            }
            release_object(def);

            if (error >= ERR::ExceptionThreshold) {
               pf::Log log(__FUNCTION__);
               log.warning("Unable to write %s.%s: %s", def->Class->ClassName, keyname, GetErrorMsg(error));
               auto prv = (prvFluid *)Lua->Script->ChildPrivate;
               prv->CaughtError = error;
               /*if (prv->ThrowErrors)*/ luaL_error(Lua, GetErrorMsg(error));
            }
         }
      }
   }
   return 0;
}

//********************************************************************************************************************

static ERR set_array(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG Values, LONG total)
{
   if (Field->Flags & FD_INT) {
      pf::vector<LONG> values((size_t)total);
      for (lua_pushnil(Lua); lua_next(Lua, Values); lua_pop(Lua, 1)) {
         LONG index = lua_tointeger(Lua, -2) - 1;
         if ((index >= 0) and (index < total)) {
            values[index] = lua_tointeger(Lua, -1);
         }
      }
      return SetArray(Object, Field->FieldID|TINT, values);
   }
   else if (Field->Flags & FD_STRING) {
      pf::vector<CSTRING> values((size_t)total);
      for (lua_pushnil(Lua); lua_next(Lua, Values); lua_pop(Lua, 1)) {
         LONG index = lua_tointeger(Lua, -2) - 1;
         if ((index >= 0) and (index < total)) {
            values[index] = lua_tostring(Lua, -1);
         }
      }
      return SetArray(Object, Field->FieldID|TSTR, values);
   }
   else if (Field->Flags & FD_STRUCT) {
      // Array structs can be set if the Lua table consists of Fluid.struct types.

      auto prv = (prvFluid *)Lua->Script->ChildPrivate;
      if (auto def = prv->Structs.find(std::string_view((CSTRING)Field->Arg)); def != prv->Structs.end()) {
         LONG aligned_size = ALIGN64(def->second.Size);
         auto structbuf = std::make_unique<UBYTE[]>(total * aligned_size);

         for (lua_pushnil(Lua); lua_next(Lua, Values); lua_pop(Lua, 1)) {
            LONG index = lua_tointeger(Lua, -2) - 1;
            if ((index >= 0) and (index < total)) {
               APTR sti = structbuf.get() + (aligned_size * index);
               LONG type = lua_type(Lua, -1);
               if (type IS LUA_TTABLE) {
                  lua_pop(Lua, 2);
                  return ERR::SetValueNotArray;
               }
               else if (type IS LUA_TUSERDATA) {
                  if (auto fs = (fstruct *)get_meta(Lua, -1, "Fluid.struct")) {
                     copymem(fs->Data, sti, fs->StructSize);
                  }
               }
               else {
                  lua_pop(Lua, 2);
                  return ERR::SetValueNotArray;
               }
            }
         }

         return SetArray(Object, Field->FieldID, structbuf.get(), total);
      }
      else return ERR::SetValueNotArray;
   }
   else return ERR::SetValueNotArray;
}

//********************************************************************************************************************

static ERR object_set_array(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG ValueIndex)
{
   LONG type = lua_type(Lua, ValueIndex);

   if (type IS LUA_TSTRING) { // Treat the source as a CSV field
      return Object->set(Field->FieldID, lua_tostring(Lua, ValueIndex));
   }
   else if (type IS LUA_TTABLE) {
      lua_settop(Lua, ValueIndex);
      LONG t = lua_gettop(Lua);
      LONG total = lua_objlen(Lua, t);

      if (total < 1024) {
         return set_array(Lua, Object, Field, t, total);
      }
      else return ERR::BufferOverflow;
   }
   else if (auto farray = (struct array *)get_meta(Lua, ValueIndex, "Fluid.array")) {
      return SetArray(Object, ((int64_t)Field->FieldID)|((int64_t)farray->Type<<32), farray->ptrPointer, farray->Total);
   }
   else return ERR::SetValueNotArray;
}

static ERR object_set_function(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG ValueIndex)
{
   LONG type = lua_type(Lua, ValueIndex);
   if (type IS LUA_TSTRING) {
      lua_getglobal(Lua, lua_tostring(Lua, ValueIndex));
      auto func = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
      return Object->set(Field->FieldID, &func);
   }
   else if (type IS LUA_TFUNCTION) {
      lua_pushvalue(Lua, ValueIndex);
      auto func = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
      return Object->set(Field->FieldID, &func);
   }
   else return ERR::SetValueNotFunction;
}

static ERR object_set_object(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG ValueIndex)
{
   if (auto def = (object *)get_meta(Lua, ValueIndex, "Fluid.obj")) {
      if (auto ptr_obj = access_object(def)) {
         ERR error = Object->set(Field->FieldID, ptr_obj);
         release_object(def);
         return error;
      }
      else return ERR::AccessObject;
   }
   else return Object->set(Field->FieldID, (APTR)NULL);
}

static ERR object_set_ptr(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG ValueIndex)
{
   auto type = lua_type(Lua, ValueIndex);

   if (type IS LUA_TSTRING) {
      return Object->set(Field->FieldID, lua_tostring(Lua, ValueIndex));
   }
   else if (type IS LUA_TNUMBER) {
      if (Field->Flags & FD_STRING) {
         return Object->set(Field->FieldID, lua_tostring(Lua, ValueIndex));
      }
      else if (lua_tointeger(Lua, ValueIndex) IS 0) {
         // Setting pointer fields with numbers is only allowed if that number evaluates to zero (NULL)
         return Object->set(Field->FieldID, (APTR)NULL);
      }
      else return ERR::SetValueNotPointer;
   }
   else if (auto array = (struct array *)get_meta(Lua, ValueIndex, "Fluid.array")) {
      return Object->set(Field->FieldID, array->ptrVoid);
   }
   else if (auto fstruct = (struct fstruct *)get_meta(Lua, ValueIndex, "Fluid.struct")) {
      return Object->set(Field->FieldID, fstruct->Data);
   }
   else if (type IS LUA_TNIL) {
      return Object->set(Field->FieldID, (APTR)NULL);
   }
   else return ERR::SetValueNotPointer;
}

static ERR object_set_double(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG ValueIndex)
{
   switch(lua_type(Lua, ValueIndex)) {
      case LUA_TNUMBER:
         return Object->set(Field->FieldID, lua_tonumber(Lua, ValueIndex));

      case LUA_TSTRING: // Allow internal string parsing to do its thing - important if the field is variable
         return Object->set(Field->FieldID, lua_tostring(Lua, ValueIndex));

      case LUA_TNIL: // Setting a numeric with nil does nothing.  Use zero to be explicit.
         return ERR::Okay;

      default:
         return ERR::SetValueNotNumeric;
   }
}

static ERR object_set_lookup(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG ValueIndex)
{
   switch(lua_type(Lua, ValueIndex)) {
      case LUA_TNUMBER: return Object->set(Field->FieldID, (LONG)lua_tointeger(Lua, ValueIndex));
      case LUA_TSTRING: return Object->set(Field->FieldID, lua_tostring(Lua, ValueIndex));
      default: return ERR::SetValueNotLookup;
   }
}

static ERR object_set_oid(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG ValueIndex)
{
   switch(lua_type(Lua, ValueIndex)) {
      default:          return ERR::SetValueNotObject;
      case LUA_TNUMBER: return Object->set(Field->FieldID, (OBJECTID)lua_tointeger(Lua, ValueIndex));
      case LUA_TNIL:    return Object->set(Field->FieldID, 0);

      case LUA_TUSERDATA: {
         if (auto def = (struct object *)get_meta(Lua, ValueIndex, "Fluid.obj")) {
            return Object->set(Field->FieldID, def->UID);
         }
         return ERR::SetValueNotObject;
      }

      case LUA_TSTRING: {
         OBJECTID id;
         if (FindObject(lua_tostring(Lua, ValueIndex), CLASSID::NIL, FOF::NIL, &id) IS ERR::Okay) {
            Object->set(Field->FieldID, id);
         }
         else {
            pf::Log log;
            log.warning("Object \"%s\" could not be found.", lua_tostring(Lua, ValueIndex));
            return ERR::Search;
         }
      }
   }

   return ERR::SetValueNotObject;
}

static ERR object_set_number(lua_State *Lua, OBJECTPTR Object, Field *Field, LONG ValueIndex)
{
   switch(lua_type(Lua, ValueIndex)) {
      case LUA_TBOOLEAN:
         return Object->set(Field->FieldID, lua_toboolean(Lua, ValueIndex));

      case LUA_TNUMBER:
         return Object->set(Field->FieldID, (int64_t)lua_tointeger(Lua, ValueIndex));

      case LUA_TSTRING: // Allow internal string parsing to do its thing - important if the field is variable
         return Object->set(Field->FieldID, lua_tostring(Lua, ValueIndex));

      case LUA_TNIL: // Setting a numeric with nil does nothing.  Use zero to be explicit.
         return ERR::Okay;

      default:
         return ERR::SetValueNotNumeric;
   }
}

//********************************************************************************************************************
// Usage: value = obj.get("Width", [Default])
//
// The default value is optional - it is used if the get request fails.  This function never throws exceptions.

static int object_get(lua_State *Lua)
{
   pf::Log log("obj.get");

   if (auto fieldname = luaL_checkstring(Lua, 1)) {
      auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");

      auto obj = access_object(def);
      if (!obj) {
         lua_pushvalue(Lua, 2); // Push the client's default value
         return 1;
      }

      OBJECTPTR target;
      if (fieldname[0] IS '$') {
         // Get field as string, useful for retrieving lookup values as their named type.
         char buffer[1024];
         if (GetFieldVariable(obj, fieldname, buffer, sizeof(buffer)) IS ERR::Okay) lua_pushstring(Lua, buffer);
         else lua_pushvalue(Lua, 2); // Push the client's default value
         release_object(def);
         return 1;
      }
      else if (auto field = FindField(obj, strihash(fieldname), &target)) {
         LONG result = 0;
         if (field->Flags & FD_ARRAY) {
            if (field->Flags & FD_RGB) result = object_get_rgb(Lua, obj_read(0, NULL, field), def);
            else result = object_get_array(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_STRUCT) {
            result = object_get_struct(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_STRING) {
            result = object_get_string(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_POINTER) {
            if (field->Flags & (FD_OBJECT|FD_LOCAL)) {
               result = object_get_object(Lua, obj_read(0, NULL, field), def);
            }
            else result = object_get_ptr(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_DOUBLE) {
            result = object_get_double(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_INT64) {
            result = object_get_large(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_INT) {
            if (field->Flags & FD_UNSIGNED) {
               result = object_get_ulong(Lua, obj_read(0, NULL, field), def);
            }
            else result = object_get_long(Lua, obj_read(0, NULL, field), def);
         }

         release_object(def);
         if (!result) lua_pushvalue(Lua, 2); // Push the client's default value
         return 1;
      }
      else { // Assume this is a custom key since FindField() failed
         char buffer[8192];

         if ((acGetKey(obj, fieldname, buffer, sizeof(buffer)) IS ERR::Okay) and (buffer[0])) {
            lua_pushstring(Lua, buffer);
         }
         else lua_pushvalue(Lua, 2); // Push the client's default value

         release_object(def);
         return 1;
      }
   }
   else return 0;
}

//********************************************************************************************************************
// Usage: value = obj.getKey("Width", [Default])
//
// As for obj.get(), but explicitly references a custom variable name.

static int object_getkey(lua_State *Lua)
{
   if (auto fieldname = luaL_checkstring(Lua, 1)) {
      auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
      ERR error;
      if (auto obj = access_object(def)) {
         char buffer[8192];
         if ((error = acGetKey(obj, fieldname, buffer, sizeof(buffer))) IS ERR::Okay) {
            lua_pushstring(Lua, buffer);
         }
         release_object(def);
      }
      else error = ERR::AccessObject;

      if (error != ERR::Okay) {
         if (lua_gettop(Lua) >= 2) lua_pushvalue(Lua, 2);
         else lua_pushnil(Lua);
      }

      return 1;
   }
   else return 0;
}

//********************************************************************************************************************
// Usage: obj.set("Width", Value)

static int object_set(lua_State *Lua)
{
   auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");

   CSTRING fieldname;
   if (!(fieldname = luaL_checkstring(Lua, 1))) return 0;

   if (auto obj = access_object(def)) {
      LONG type = lua_type(Lua, 2);
      ULONG fieldhash = strihash(fieldname);

      ERR error;
      if (type IS LUA_TNUMBER) error = obj->set(fieldhash, luaL_checknumber(Lua, 2));
      else error = obj->set(fieldhash, luaL_optstring(Lua, 2, NULL));

      release_object(def);
      lua_pushinteger(Lua, LONG(error));
      report_action_error(Lua, def, "set", error);
      return 1;
   }
   else return 0;
}

//********************************************************************************************************************
// Usage: obj.setKey("Width", "Value")

static int object_setkey(lua_State *Lua)
{
   auto def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
   if (auto fieldname = luaL_checkstring(Lua, 1)) {
      auto value = luaL_optstring(Lua, 2, NULL);
      if (auto obj = access_object(def)) {
         ERR error = acSetKey(obj, fieldname, value);
         release_object(def);
         lua_pushinteger(Lua, LONG(error));
         report_action_error(Lua, def, "setKey", error);
         return 1;
      }
   }

   return 0;
}

//********************************************************************************************************************

static ERR set_object_field(lua_State *Lua, OBJECTPTR obj, CSTRING FName, LONG ValueIndex)
{
   pf::Log log("obj.setfield");

   LONG type = lua_type(Lua, ValueIndex);

   if (FName[0] IS '_') return acSetKey(obj, FName+1, lua_tostring(Lua, ValueIndex));

   OBJECTPTR target;
   if (auto field = FindField(obj, strihash(FName), &target)) {
      if (field->Flags & FD_ARRAY) {
         struct array *farray;

         if (type IS LUA_TSTRING) { // Treat the source as a CSV field
            return target->set(field->FieldID, lua_tostring(Lua, ValueIndex));
         }
         else if (type IS LUA_TTABLE) {
            lua_settop(Lua, ValueIndex);
            LONG t = lua_gettop(Lua);
            LONG total = lua_objlen(Lua, t);

            if (total < 1024) {
               return set_array(Lua, target, field, t, total);
            }
            else return ERR::BufferOverflow;
         }
         else if ((farray = (struct array *)get_meta(Lua, ValueIndex, "Fluid.array"))) {
            return SetArray(target, ((int64_t)field->FieldID)|((int64_t)farray->Type<<32), farray->ptrPointer, farray->Total);
         }
         else return ERR::SetValueNotArray;
      }
      else if (field->Flags & FD_FUNCTION) {
         if (type IS LUA_TSTRING) {
            lua_getglobal(Lua, lua_tostring(Lua, ValueIndex));
            auto func = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
            return target->set(field->FieldID, &func);
         }
         else if (type IS LUA_TFUNCTION) {
            lua_pushvalue(Lua, ValueIndex);
            auto func = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
            return target->set(field->FieldID, &func);
         }
         else return ERR::SetValueNotFunction;
      }
      else if (field->Flags & FD_POINTER) {
         if (field->Flags & (FD_OBJECT|FD_LOCAL)) { // Writing to an integral is permitted if marked as writeable.
            if (auto object = (struct object *)get_meta(Lua, ValueIndex, "Fluid.obj")) {
               OBJECTPTR ptr_obj;
               if (object->ObjectPtr) {
                  return target->set(field->FieldID, object->ObjectPtr);
               }
               else if ((ptr_obj = (OBJECTPTR)access_object(object))) {
                  ERR error = target->set(field->FieldID, object->ObjectPtr);
                  release_object(object);
                  return error;
               }
               else return ERR::Failed;
            }
            else return target->set(field->FieldID, (APTR)NULL);
         }
         else if (type IS LUA_TSTRING) {
            return target->set(field->FieldID, lua_tostring(Lua, ValueIndex));
         }
         else if (type IS LUA_TNUMBER) {
            if (field->Flags & FD_STRING) {
               return obj->set(field->FieldID, lua_tostring(Lua, ValueIndex));
            }
            else if (lua_tointeger(Lua, ValueIndex) IS 0) {
               // Setting pointer fields with numbers is only allowed if that number evaluates to zero (NULL)
               return obj->set(field->FieldID, (APTR)NULL);
            }
            else return ERR::SetValueNotPointer;
         }
         else if (auto array = (struct array *)get_meta(Lua, ValueIndex, "Fluid.array")) {
            return obj->set(field->FieldID, array->ptrVoid);
         }
         else if (auto fs = (fstruct *)get_meta(Lua, ValueIndex, "Fluid.struct")) {
            return obj->set(field->FieldID, fs->Data);
         }
         else if (type IS LUA_TNIL) {
            return obj->set(field->FieldID, (APTR)NULL);
         }
         else return ERR::SetValueNotPointer;
      }
      else if (field->Flags & (FD_DOUBLE|FD_FLOAT)) {
         switch(type) {
            case LUA_TNUMBER:
               return target->set(field->FieldID, lua_tonumber(Lua, ValueIndex));

            case LUA_TSTRING: // Allow internal string parsing to do its thing - important if the field is variable
               return target->set(field->FieldID, lua_tostring(Lua, ValueIndex));

            case LUA_TNIL: // Setting a numeric with nil does nothing.  Use zero to be explicit.
               return ERR::Okay;

            default:
               return ERR::SetValueNotNumeric;
         }
      }
      else if (field->Flags & (FD_FLAGS|FD_LOOKUP)) {
         switch(type) {
            case LUA_TNUMBER:
               return target->set(field->FieldID, (LONG)lua_tointeger(Lua, ValueIndex));

            case LUA_TSTRING:
               return target->set(field->FieldID, lua_tostring(Lua, ValueIndex));

            default:
               return ERR::SetValueNotLookup;
         }
      }
      else if (field->Flags & FD_OBJECT) { // Object ID
         switch(type) {
            case LUA_TNUMBER:
               return target->set(field->FieldID, (OBJECTID)lua_tointeger(Lua, ValueIndex));

            case LUA_TUSERDATA: {
               if (auto object = (struct object *)get_meta(Lua, ValueIndex, "Fluid.obj")) {
                  return target->set(field->FieldID, object->UID);
               }
               return ERR::SetValueNotObject;
            }

            case LUA_TSTRING: {
               OBJECTID id;
               if (FindObject(lua_tostring(Lua, ValueIndex), CLASSID::NIL, FOF::NIL, &id) IS ERR::Okay) {
                  target->set(field->FieldID, id);
               }
               else {
                  log.warning("Object \"%s\" could not be found.", lua_tostring(Lua, ValueIndex));
                  return ERR::Search;
               }
            }

            case LUA_TNIL:
               return obj->set(field->FieldID, 0);

            default:
               return ERR::SetValueNotObject;
         }
      }
      else if (field->Flags & (FD_INT|FD_INT64)) {
         switch(type) {
            case LUA_TBOOLEAN:
               return target->set(field->FieldID, lua_toboolean(Lua, ValueIndex));

            case LUA_TNUMBER:
               return target->set(field->FieldID, (int64_t)lua_tointeger(Lua, ValueIndex));

            case LUA_TSTRING: // Allow internal string parsing to do its thing - important if the field is variable
               return target->set(field->FieldID, lua_tostring(Lua, ValueIndex));

            case LUA_TNIL: // Setting a numeric with nil does nothing.  Use zero to be explicit.
               return ERR::Okay;

            default:
               return ERR::SetValueNotNumeric;
         }
      }
      else return ERR::UnsupportedField;
   }
   else return ERR::UnsupportedField;
}

//********************************************************************************************************************
// Support for direct field indexing.  These functions are utilised if a field reference is easily resolved to a hash.

static int object_get_id(lua_State *Lua, const obj_read &Handle, object *Def)
{
   lua_pushnumber(Lua, Def->UID);
   return 1;
}

static int object_get_array(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      LONG total;
      APTR list;
      if ((error = GetFieldArray(obj, field->FieldID, &list, &total)) IS ERR::Okay) {
         if (total <= 0) lua_pushnil(Lua);
         else if (field->Flags & FD_STRING) {
            make_table(Lua, FD_STRING, total, list);
         }
         else if (field->Flags & (FD_INT|FD_INT64|FD_FLOAT|FD_DOUBLE|FD_POINTER|FD_BYTE|FD_WORD|FD_STRUCT)) {
            make_any_table(Lua, field->Flags, (CSTRING)field->Arg, total, list);
         }
         else {
            pf::Log log(__FUNCTION__);
            log.warning("Invalid array type for '%s', flags: $%.8x", field->Name, field->Flags);
            error = ERR::FieldTypeMismatch;
         }
      }

      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_rgb(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      STRING rgb;
      if (((error = obj->get(field->FieldID, rgb)) IS ERR::Okay) and (rgb)) lua_pushstring(Lua, rgb);
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_struct(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      if (field->Arg) {
         APTR result;
         if ((error = obj->getPtr(field->FieldID, &result)) IS ERR::Okay) {
            if (result) { // Structs are copied into standard Lua tables.
               if (field->Flags & FD_RESOURCE) {
                   push_struct(Lua->Script, result, (CSTRING)field->Arg, (field->Flags & FD_ALLOC) ? TRUE : FALSE, TRUE);
               }
               else named_struct_to_table(Lua, (CSTRING)field->Arg, result);
            }
            else lua_pushnil(Lua);
         }
      }
      else {
         pf::Log log(__FUNCTION__);
         log.warning("No struct name reference for field %s in class %s.", field->Name, obj->Class->ClassName);
         error = ERR::Failed;
      }
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_string(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      STRING result;
      if ((error = obj->get(field->FieldID, result)) IS ERR::Okay) {
         lua_pushstring(Lua, result);
         if (field->Flags & FD_ALLOC) FreeResource(result);
      }
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_ptr(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      APTR result;
      if ((error = obj->getPtr(field->FieldID, &result)) IS ERR::Okay) lua_pushlightuserdata(Lua, result);
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_object(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      OBJECTPTR objval;
      if ((error = obj->getPtr(field->FieldID, &objval)) IS ERR::Okay) {
         if (objval) push_object(Lua, objval);
         else lua_pushnil(Lua);
      }
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_double(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      DOUBLE result;
      if ((error = obj->get(field->FieldID, result)) IS ERR::Okay) lua_pushnumber(Lua, result);
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_large(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      int64_t result;
      if ((error = obj->get(field->FieldID, result)) IS ERR::Okay) lua_pushnumber(Lua, result);
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_long(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      LONG result;
      if ((error = obj->get(field->FieldID, result)) IS ERR::Okay) {
         if (field->Flags & FD_OBJECT) push_object_id(Lua, result);
         else lua_pushinteger(Lua, result);
      }
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}

static int object_get_ulong(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      ULONG result;
      if ((error = obj->get(field->FieldID, (LONG &)result)) IS ERR::Okay) {
         lua_pushnumber(Lua, result);
      }
      release_object(Def);
   }
   else error = ERR::AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error != ERR::Okay ? 0 : 1;
}
