
//********************************************************************************************************************
// Usage: object.field = newvalue
//
// Custom fields can be referenced by using _ as a prefix.

static int object_newindex(lua_State *Lua)
{
   if (auto object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      if (auto fieldname = luaL_checkstring(Lua, 2)) {
         if (auto obj = access_object(object)) {
            ERROR error = set_object_field(Lua, obj, fieldname, 3);
            release_object(object);
            if (error >= ERR_ExceptionThreshold) {
               auto prv = (prvFluid *)Lua->Script->ChildPrivate;
               prv->CaughtError = error;
               luaL_error(Lua, GetErrorMsg(error));
            }
         }
      }
   }
   return 0;
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
         if (!GetFieldVariable(obj, fieldname, buffer, sizeof(buffer))) lua_pushstring(Lua, buffer);
         else lua_pushvalue(Lua, 2); // Push the client's default value
         release_object(def);
         return 1;
      }
      else if (auto field = FindField(obj, StrHash(fieldname), &target)) {
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
            if (field->Flags & (FD_OBJECT|FD_INTEGRAL)) {
               result = object_get_object(Lua, obj_read(0, NULL, field), def);
            }
            else result = object_get_ptr(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_DOUBLE) {
            result = object_get_double(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_LARGE) {
            result = object_get_large(Lua, obj_read(0, NULL, field), def);
         }
         else if (field->Flags & FD_LONG) {
            if (field->Flags & FD_UNSIGNED) {
               result = object_get_ulong(Lua, obj_read(0, NULL, field), def);
            }
            else result = object_get_long(Lua, obj_read(0, NULL, field), def);
         }

         release_object(def);
         if (!result) lua_pushvalue(Lua, 2); // Push the client's default value
         return 1;
      }
      else { // Assume this is a custom variable field since FindField() failed
         char buffer[8192];

         if ((!GetVar(obj, fieldname, buffer, sizeof(buffer))) and (buffer[0])) {
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
// Usage: value = obj.getVar("Width", [Default])
//
// As for obj.get(), but explicitly references a custom variable name.

static int object_getvar(lua_State *Lua)
{
   if (auto fieldname = luaL_checkstring(Lua, 1)) {
      auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
      ERROR error;
      if (auto obj = access_object(def)) {
         char buffer[8192];
         if (!(error = GetVar(obj, fieldname, buffer, sizeof(buffer)))) {
            lua_pushstring(Lua, buffer);
         }
         release_object(def);
      }
      else error = ERR_AccessObject;

      if (error) {
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
      ULONG fieldhash = StrHash(fieldname);

      ERROR error;
      if (type IS LUA_TNUMBER) error = obj->set(fieldhash, luaL_checknumber(Lua, 2));
      else error = obj->set(fieldhash, luaL_optstring(Lua, 2, NULL));

      release_object(def);
      lua_pushinteger(Lua, error);
      report_action_error(Lua, def, "set", error);
      return 1;
   }
   else return 0;
}

//********************************************************************************************************************
// Usage: obj.setVar("Width", "Value")

static int object_setvar(lua_State *Lua)
{
   auto def = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
   if (auto fieldname = luaL_checkstring(Lua, 1)) {
      auto value = luaL_optstring(Lua, 2, NULL);
      if (auto obj = access_object(def)) {
         ERROR error = acSetVar(obj, fieldname, value);
         release_object(def);
         lua_pushinteger(Lua, error);
         report_action_error(Lua, def, "setVar", error);
         return 1;
      }
   }

   return 0;
}

//********************************************************************************************************************

static ERROR set_object_field(lua_State *Lua, OBJECTPTR obj, CSTRING FName, LONG ValueIndex)
{
   pf::Log log("obj.setfield");

   LONG type = lua_type(Lua, ValueIndex);

   if (FName[0] IS '_') return acSetVar(obj, FName+1, lua_tostring(Lua, ValueIndex));

   OBJECTPTR target;
   if (auto field = FindField(obj, StrHash(FName), &target)) {
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
               if (field->Flags & FD_LONG) {
                  pf::vector<LONG> values((size_t)total);
                  for (lua_pushnil(Lua); lua_next(Lua, t); lua_pop(Lua, 1)) {
                     LONG index = lua_tointeger(Lua, -2) - 1;
                     if ((index >= 0) and (index < total)) {
                        values[index] = lua_tointeger(Lua, -1);
                     }
                  }
                  return SetArray(target, field->FieldID|TLONG, values);
               }
               else if (field->Flags & FD_STRING) {
                  pf::vector<CSTRING> values((size_t)total);
                  for (lua_pushnil(Lua); lua_next(Lua, t); lua_pop(Lua, 1)) {
                     LONG index = lua_tointeger(Lua, -2) - 1;
                     if ((index >= 0) and (index < total)) {
                        values[index] = lua_tostring(Lua, -1);
                     }
                  }
                  return SetArray(target, field->FieldID|TSTR, values);
               }
               else if (field->Flags & FD_STRUCT) {
                  // Array structs can be set if the Lua table consists of Fluid.struct types.

                  auto prv = (prvFluid *)Lua->Script->ChildPrivate;
                  auto def = prv->Structs.find(struct_name(std::string((CSTRING)field->Arg)));
                  if (def != prv->Structs.end()) {
                     LONG aligned_size = ALIGN64(def->second.Size);
                     UBYTE structbuf[total * aligned_size];

                     for (lua_pushnil(Lua); lua_next(Lua, t); lua_pop(Lua, 1)) {
                        LONG index = lua_tointeger(Lua, -2) - 1;
                        if ((index >= 0) and (index < total)) {
                           APTR sti = structbuf + (aligned_size * index);
                           LONG type = lua_type(Lua, -1);
                           if (type IS LUA_TTABLE) {
                              lua_pop(Lua, 2);
                              return ERR_SetValueNotArray;
                           }
                           else if (type IS LUA_TUSERDATA) {
                              if (auto fstruct = (struct fstruct *)get_meta(Lua, ValueIndex, "Fluid.struct")) {
                                 CopyMemory(fstruct->Data, sti, fstruct->StructSize);
                              }
                           }
                           else {
                              lua_pop(Lua, 2);
                              return ERR_SetValueNotArray;
                           }
                        }
                     }

                     return SetArray(target, field->FieldID, structbuf, total);
                  }
                  else return ERR_SetValueNotArray;
               }
               else return ERR_SetValueNotArray;
            }
            else return ERR_BufferOverflow;
         }
         else if ((farray = (struct array *)get_meta(Lua, ValueIndex, "Fluid.array"))) {
            return SetArray(target, ((LARGE)field->FieldID)|((LARGE)farray->Type<<32), farray->ptrPointer, farray->Total);
         }
         else return ERR_SetValueNotArray;
      }
      else if (field->Flags & FD_FUNCTION) {
         if (type IS LUA_TSTRING) {
            lua_getglobal(Lua, lua_tostring(Lua, ValueIndex));
            auto func = make_function_script(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
            return target->set(field->FieldID, &func);
         }
         else if (type IS LUA_TFUNCTION) {
            lua_pushvalue(Lua, ValueIndex);
            auto func = make_function_script(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
            return target->set(field->FieldID, &func);
         }
         else return ERR_SetValueNotFunction;
      }
      else if (field->Flags & FD_POINTER) {
         if (field->Flags & (FD_OBJECT|FD_INTEGRAL)) { // Writing to an integral is permitted if marked as writeable.
            if (auto object = (struct object *)get_meta(Lua, ValueIndex, "Fluid.obj")) {
               OBJECTPTR ptr_obj;
               if (object->ObjectPtr) {
                  return target->set(field->FieldID, object->ObjectPtr);
               }
               else if ((ptr_obj = (OBJECTPTR)access_object(object))) {
                  ERROR error = target->set(field->FieldID, object->ObjectPtr);
                  release_object(object);
                  return error;
               }
               else return ERR_Failed;
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
            else return ERR_SetValueNotPointer;
         }
         else if (auto memory = (struct memory *)get_meta(Lua, ValueIndex, "Fluid.mem")) {
            return obj->set(field->FieldID, memory->Memory);
         }
         else if (auto fstruct = (struct fstruct *)get_meta(Lua, ValueIndex, "Fluid.struct")) {
            return obj->set(field->FieldID, fstruct->Data);
         }
         else if (type IS LUA_TNIL) {
            return obj->set(field->FieldID, (APTR)NULL);
         }
         else return ERR_SetValueNotPointer;
      }
      else if (field->Flags & (FD_DOUBLE|FD_FLOAT)) {
         switch(type) {
            case LUA_TNUMBER:
               return target->set(field->FieldID, lua_tonumber(Lua, ValueIndex));

            case LUA_TSTRING: // Allow internal string parsing to do its thing - important if the field is variable
               return target->set(field->FieldID, lua_tostring(Lua, ValueIndex));

            case LUA_TNIL: // Setting a numeric with nil does nothing.  Use zero to be explicit.
               return ERR_Okay;

            default:
               return ERR_SetValueNotNumeric;
         }
      }
      else if (field->Flags & (FD_FLAGS|FD_LOOKUP)) {
         switch(type) {
            case LUA_TNUMBER:
               return target->set(field->FieldID, lua_tonumber(Lua, ValueIndex));

            case LUA_TSTRING:
               return target->set(field->FieldID, lua_tostring(Lua, ValueIndex));

            default:
               return ERR_SetValueNotLookup;
         }
      }
      else if (field->Flags & FD_OBJECT) { // Object ID
         switch(type) {
            case LUA_TNUMBER:
               return target->set(field->FieldID, lua_tonumber(Lua, ValueIndex));

            case LUA_TUSERDATA: {
               if (auto object = (struct object *)get_meta(Lua, ValueIndex, "Fluid.obj")) {
                  return target->set(field->FieldID, object->UID);
               }
               return ERR_SetValueNotObject;
            }

            case LUA_TSTRING: {
               OBJECTID id;
               if (!FindObject(lua_tostring(Lua, ValueIndex), 0, 0, &id)) {
                  target->set(field->FieldID, id);
               }
               else {
                  log.warning("Object \"%s\" could not be found.", lua_tostring(Lua, ValueIndex));
                  return ERR_Search;
               }
            }

            case LUA_TNIL:
               return obj->set(field->FieldID, 0);

            default:
               return ERR_SetValueNotObject;
         }
      }
      else if (field->Flags & (FD_LONG|FD_LARGE)) {
         switch(type) {
            case LUA_TNUMBER:
               return target->set(field->FieldID, lua_tonumber(Lua, ValueIndex));

            case LUA_TSTRING: // Allow internal string parsing to do its thing - important if the field is variable
               return target->set(field->FieldID, lua_tostring(Lua, ValueIndex));

            case LUA_TNIL: // Setting a numeric with nil does nothing.  Use zero to be explicit.
               return ERR_Okay;

            default:
               return ERR_SetValueNotNumeric;
         }
      }
      else return ERR_UnsupportedField;
   }
   else return ERR_UnsupportedField;
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
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      LONG total;
      APTR list;
      if (!(error = GetFieldArray(obj, field->FieldID, &list, &total))) {
         if (total <= 0) lua_pushnil(Lua);
         else if (field->Flags & FD_STRING) {
            make_table(Lua, FD_STRING, total, list);
         }
         else if (field->Flags & (FD_LONG|FD_LARGE|FD_FLOAT|FD_DOUBLE|FD_POINTER|FD_BYTE|FD_WORD|FD_STRUCT)) {
            make_any_table(Lua, field->Flags, (CSTRING)field->Arg, total, list);
         }
         else {
            pf::Log log(__FUNCTION__);
            log.warning("Invalid array type for '%s', flags: $%.8x", field->Name, field->Flags);
            error = ERR_FieldTypeMismatch;
         }
      }

      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_rgb(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      STRING rgb;
      if ((!(error = obj->get(field->FieldID, &rgb))) and (rgb)) lua_pushstring(Lua, rgb);
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_struct(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      if (field->Arg) {
         APTR result;
         if (!(error = obj->getPtr(field->FieldID, &result))) {
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
         error = ERR_Failed;
      }
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_string(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      STRING result;
      if (!(error = obj->get(field->FieldID, &result))) lua_pushstring(Lua, result);
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_ptr(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      APTR result;
      if (!(error = obj->getPtr(field->FieldID, &result))) lua_pushlightuserdata(Lua, result);
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_object(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      OBJECTPTR objval;
      if (!(error = obj->getPtr(field->FieldID, &objval))) {
         if (objval) push_object(Lua, objval);
         else lua_pushnil(Lua);
      }
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_double(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      DOUBLE result;
      if (!(error = obj->get(field->FieldID, &result))) lua_pushnumber(Lua, result);
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_large(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      LARGE result;
      if (!(error = obj->get(field->FieldID, &result))) lua_pushnumber(Lua, result);
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_long(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      LONG result;
      if (!(error = obj->get(field->FieldID, &result))) {
         if (field->Flags & FD_OBJECT) push_object_id(Lua, result);
         else lua_pushinteger(Lua, result);
      }
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}

static int object_get_ulong(lua_State *Lua, const obj_read &Handle, object *Def)
{
   ERROR error;
   if (auto obj = access_object(Def)) {
      auto field = (Field *)(Handle.Data);
      ULONG result;
      if (!(error = obj->get(field->FieldID, (LONG *)&result))) {
         lua_pushnumber(Lua, result);
      }
      release_object(Def);
   }
   else error = ERR_AccessObject;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   prv->CaughtError = error;
   return error ? 0 : 1;
}
