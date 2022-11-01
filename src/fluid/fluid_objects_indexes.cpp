
//****************************************************************************
// Usage: object.field = newvalue
//
// Custom fields can be referenced by using _ as the field name suffix.

static int object_newindex(lua_State *Lua)
{
   struct object *object;
   if ((object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      CSTRING fieldname;
      if ((fieldname = luaL_checkstring(Lua, 2))) {
         OBJECTPTR obj;
         if ((obj = access_object(object))) {
            ERROR error = set_object_field(Lua, obj, fieldname, 3);
            release_object(object);
            if (error >= ERR_ExceptionThreshold) {
               auto prv = (prvFluid *)Lua->Script->ChildPrivate;
               prv->CaughtError = error;
               luaL_error(Lua, GetErrorMsg(error));
            }
            return 0;
         }
      }
   }
   return 0;
}

//****************************************************************************
// Usage: value = obj.get("Width", [Default])
//
// The default value is optional - it is used if the get request fails.  This function never throws exceptions.

static int object_get(lua_State *Lua)
{
   parasol::Log log;
   struct object *object;

   if (!(object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   CSTRING fieldname;
   if ((fieldname = luaL_checkstring(Lua, 1))) {
      log.trace("obj.get('%s')", fieldname);
      ERROR error = getfield(Lua, object, fieldname);
      if (!error) return 1;
      lua_pushvalue(Lua, 2);
      return 1;
   }
   else log.trace("obj.get(NIL)");

   return 0;
}

//****************************************************************************
// Usage: value = obj.getVar("Width", [Default])
//
// As for obj.get(), but explicitly references a custom variable name.

static int object_getvar(lua_State *Lua)
{
   parasol::Log log;
   struct object *object;

   if (!(object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   CSTRING fieldname;
   if ((fieldname = luaL_checkstring(Lua, 1))) {
      log.trace("obj.getVar('%s')", fieldname);

      OBJECTPTR obj;
      ERROR error;
      if ((obj = access_object(object))) {
         char buffer[8192];
         if (!(error = GetVar(obj, fieldname, buffer, sizeof(buffer)))) {
            lua_pushstring(Lua, buffer);
         }
         release_object(object);
      }
      else error = ERR_AccessObject;

      if (error) {
         if (lua_gettop(Lua) >= 2) lua_pushvalue(Lua, 2);
         else lua_pushnil(Lua);
      }

      return 1;
   }
   else log.trace("obj.var(NIL)");

   return 0;
}

//****************************************************************************
// Usage: obj.set("Width", Value)

static int object_set(lua_State *Lua)
{
   parasol::Log log;

   log.trace("obj.set()");

   struct object *object;
   if (!(object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   CSTRING fieldname;
   if (!(fieldname = luaL_checkstring(Lua, 1))) return 0;

   OBJECTPTR obj;
   if ((obj = access_object(object))) {
      LONG type = lua_type(Lua, 2);
      ULONG fieldhash = StrHash(fieldname, 0);

      ERROR error;
      if (type IS LUA_TNUMBER) error = SetDouble(obj, fieldhash, luaL_checknumber(Lua, 2));
      else error = SetString(obj, fieldhash, luaL_optstring(Lua, 2, NULL));

      release_object(object);
      lua_pushinteger(Lua, error);

      if (error >= ERR_ExceptionThreshold) {
         auto prv = (prvFluid *)Lua->Script->ChildPrivate;
         prv->CaughtError = error;
         luaL_error(prv->Lua, GetErrorMsg(error));
      }

      return 1;
   }

   return 0;
}

//****************************************************************************
// Usage: obj.setVar("Width", "Value")

static int object_setvar(lua_State *Lua)
{
   parasol::Log log;

   log.msg("obj.setVar()");

   struct object *object;
   if (!(object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Expected object.");
      return 0;
   }

   CSTRING fieldname;
   if ((fieldname = luaL_checkstring(Lua, 1))) {
      CSTRING value = luaL_optstring(Lua, 2, NULL);

      OBJECTPTR obj;
      if ((obj = access_object(object))) {
         ERROR error = acSetVar(obj, fieldname, value);
         release_object(object);
         lua_pushinteger(Lua, error);

         if (error >= ERR_ExceptionThreshold) {
            auto prv = (prvFluid *)Lua->Script->ChildPrivate;
            if (prv->Catch) {
               prv->CaughtError = error;
               luaL_error(prv->Lua, GetErrorMsg(error));
            }
         }

         return 1;
      }
   }

   return 0;
}

//****************************************************************************
// If successful, a value is pushed on the stack and ERR_Okay is returned.  If any other error code is returned,
// the stack is unmodified.

static ERROR getfield(lua_State *Lua, struct object *object, CSTRING FName)
{
   parasol::Log log("obj.get");

   log.traceBranch("#%d, Field: %s", object->ObjectID, FName);

   OBJECTPTR obj;
   if (!(obj = access_object(object))) return log.warning(ERR_AccessObject);

   OBJECTPTR src;
   Field *field;
   ERROR error = ERR_Okay;
   if (FName[0] IS '$') {
      char buffer[1024];
      if (!(error = GetFieldVariable(obj, FName, buffer, sizeof(buffer)))) lua_pushstring(Lua, buffer);
   }
   else if ((FName[0] IS 'i') and (FName[1] IS 'd') and (!FName[2])) {
      // Note that if the object actually has a defined ID field in its structure, the Lua code can read it
      // by using an uppercase 'ID'.
      lua_pushnumber(Lua, obj->UID);
   }
   else if ((field = FindField(obj, StrHash(FName, FALSE), &src))) {
      if (field->Flags & FD_ARRAY) {
         if (field->Flags & FD_RGB) {
            STRING rgb;
            if ((!(error = GetString(src, field->FieldID, &rgb))) and (rgb)) {
               lua_pushstring(Lua, rgb);
            }
         }
         else {
            LONG total;
            APTR list;
            if (!(error = GetFieldArray(src, field->FieldID, &list, &total))) {
               if (total <= 0) {
                  lua_pushnil(Lua);
               }
               else if (field->Flags & FD_STRING) {
                  make_table(Lua, FD_STRING, total, list);
               }
               else if (field->Flags & (FD_LONG|FD_LARGE|FD_FLOAT|FD_DOUBLE|FD_POINTER|FD_BYTE|FD_WORD|FD_STRUCT)) {
                  make_any_table(Lua, field->Flags, (CSTRING)field->Arg, total, list);
               }
               else {
                  log.warning("Invalid array type for '%s', flags: $%.8x", FName, field->Flags);
                  error = ERR_FieldTypeMismatch;
               }
            }
         }
      }
      else if (field->Flags & FD_STRUCT) { // Structs are copied into standard Lua tables.
         APTR result;
         if (field->Arg) {
            if (!(error = GetPointer(src, field->FieldID, &result))) {
               if (result) {
                  if (field->Flags & FD_RESOURCE) {
                      push_struct(Lua->Script, result, (CSTRING)field->Arg, (field->Flags & FD_ALLOC) ? TRUE : FALSE, TRUE);
                  }
                  else named_struct_to_table(Lua, (CSTRING)field->Arg, result);
               }
               else lua_pushnil(Lua);
            }
         }
         else {
            log.warning("No struct name reference for field %s in class %s.", field->Name, src->Class->ClassName);
            error = ERR_Failed;
         }
      }
      else if (field->Flags & FD_STRING) {
         STRING result;
         if (!(error = GetString(src, field->FieldID, &result))) lua_pushstring(Lua, result);
      }
      else if (field->Flags & FD_POINTER) {
         if (field->Flags & (FD_OBJECT|FD_INTEGRAL)) {
            OBJECTPTR obj;
            if (!(error = GetPointer(src, field->FieldID, &obj))) {
               if (obj) push_object(Lua, obj);
               else lua_pushnil(Lua);
            }
         }
         else {
            APTR result;
            if (!(error = GetPointer(src, field->FieldID, &result))) lua_pushlightuserdata(Lua, result);
         }
      }
      else if (field->Flags & FD_DOUBLE) {
         DOUBLE result;
         if (!(error = GetDouble(src, field->FieldID, &result))) lua_pushnumber(Lua, result);
      }
      else if (field->Flags & FD_LARGE) {
         LARGE result;
         if (!(error = GetLarge(src, field->FieldID, &result))) lua_pushnumber(Lua, result);
      }
      else if (field->Flags & FD_LONG) {
         if (field->Flags & FD_UNSIGNED) {
            ULONG result;
            if (!(error = GetLong(src, field->FieldID, (LONG *)&result))) {
               lua_pushnumber(Lua, result);
            }
         }
         else {
            LONG result;
            if (!(error = GetLong(src, field->FieldID, &result))) {
               if (field->Flags & FD_OBJECT) push_object_id(Lua, result);
               else lua_pushinteger(Lua, result);
            }
         }
      }
      else error = ERR_NoSupport;
   }
   else {
      char buffer[8192];

      // Assume this is a custom variable field since FindField() failed
      if ((!(error = GetVar(obj, FName, buffer, sizeof(buffer)))) and (buffer[0])) {
         lua_pushstring(Lua, buffer);
      }
      else if (error IS ERR_NoSupport) {
         log.msg("Field %s does not exist.", FName); // Not fatal, as testing for fields is legitimate
      }
   }

   release_object(object);
   return error;
}

//****************************************************************************
// Note that SetFieldEval() will translate object references and computations in the string.
// Prefixing the field name with '_' forces the field to be set as a custom variable.

static ERROR set_object_field(lua_State *Lua, OBJECTPTR obj, CSTRING FName, LONG ValueIndex)
{
   parasol::Log log("obj.setfield");

   if (FName[0] IS '_') {
      char bufname[80];
      WORD i;
      bufname[0] = '@';
      for (i=1; ((size_t)i < sizeof(bufname)) and (FName[i]); i++) bufname[i] = FName[i];
      bufname[i] = 0;
      return SetFieldEval(obj, bufname, lua_tostring(Lua, ValueIndex));
   }

   LONG type = lua_type(Lua, ValueIndex);

   OBJECTPTR src;
   Field *field;
   if ((field = FindField(obj, StrHash(FName, FALSE), &src))) {
      log.traceBranch("Field: %s, Flags: $%.8x, (type: %s)", FName, field->Flags, lua_typename(Lua, type));

      if (field->Flags & FD_ARRAY) {
         struct array *farray;

         if (type IS LUA_TSTRING) { // Treat the source as a CSV field
            return SetFieldEval(src, FName, lua_tostring(Lua, ValueIndex));
         }
         else if (type IS LUA_TTABLE) {
            lua_settop(Lua, ValueIndex);
            LONG t = lua_gettop(Lua);
            LONG total = lua_objlen(Lua, t);

            if (total < 1024) {
               if (field->Flags & FD_LONG) {
                  LONG values[total];
                  ClearMemory(values, sizeof(LONG) * total);
                  for (lua_pushnil(Lua); lua_next(Lua, t); lua_pop(Lua, 1)) {
                     LONG index = lua_tointeger(Lua, -2) - 1;
                     if ((index >= 0) and (index < total)) {
                        values[index] = lua_tointeger(Lua, -1);
                     }
                  }
                  return SetArray(src, field->FieldID|TLONG, values, total);
               }
               else if (field->Flags & FD_STRING) {
                  CSTRING values[total];
                  ClearMemory(values, sizeof(CSTRING) * total);
                  for (lua_pushnil(Lua); lua_next(Lua, t); lua_pop(Lua, 1)) {
                     LONG index = lua_tointeger(Lua, -2) - 1;
                     if ((index >= 0) and (index < total)) {
                        values[index] = lua_tostring(Lua, -1);
                     }
                  }
                  return SetArray(src, field->FieldID|TSTR, values, total);
               }
               else if (field->Flags & FD_STRUCT) {
                  // Array structs can be set if the Lua table consists of Fluid.struct types.

                  auto prv = (prvFluid *)Lua->Script->ChildPrivate;
                  struct structentry *def;
                  if (!VarGet(prv->Structs, (CSTRING)field->Arg, &def, NULL)) {
                     LONG aligned_size = ALIGN64(def->Size);
                     UBYTE structbuf[total * aligned_size];

                     for (lua_pushnil(Lua); lua_next(Lua, t); lua_pop(Lua, 1)) {
                        LONG index = lua_tointeger(Lua, -2) - 1;
                        if ((index >= 0) and (index < total)) {
                           APTR sti = structbuf + (aligned_size * index);
                           LONG type = lua_type(Lua, -1);
                           if (type IS LUA_TTABLE) {
                              lua_pop(Lua, 2);
                              return ERR_FieldTypeMismatch;
                           }
                           else if (type IS LUA_TUSERDATA) {
                              struct fstruct *fstruct;
                              if ((fstruct = (struct fstruct *)get_meta(Lua, ValueIndex, "Fluid.struct"))) {
                                 CopyMemory(fstruct->Data, sti, fstruct->StructSize);
                              }
                           }
                           else {
                              lua_pop(Lua, 2);
                              return ERR_FieldTypeMismatch;
                           }
                        }
                     }

                     return SetArray(src, field->FieldID, structbuf, total);
                  }
                  else return ERR_FieldTypeMismatch;
               }
               else return ERR_FieldTypeMismatch;
            }
            else return ERR_BufferOverflow;
         }
         else if ((farray = (struct array *)get_meta(Lua, ValueIndex, "Fluid.array"))) {
            return SetArray(src, ((LARGE)field->FieldID)|((LARGE)farray->Type<<32), farray->ptrPointer, farray->Total);
         }
         else return ERR_FieldTypeMismatch;
      }
      else if (field->Flags & FD_FUNCTION) {
         if (type IS LUA_TSTRING) {
            FUNCTION func;
            lua_getglobal(Lua, lua_tostring(Lua, ValueIndex));
            SET_FUNCTION_SCRIPT(func, Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
            return SetFunction(src, field->FieldID, &func);
         }
         else if (type IS LUA_TFUNCTION) {
            FUNCTION func;
            lua_pushvalue(Lua, ValueIndex);
            SET_FUNCTION_SCRIPT(func, Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
            return SetFunction(src, field->FieldID, &func);
         }
         else return ERR_FieldTypeMismatch;
      }
      else if (field->Flags & FD_POINTER) {
         struct memory *memory;
         struct fstruct *fstruct;

         if (field->Flags & (FD_OBJECT|FD_INTEGRAL)) { // Writing to an integral is permitted if marked as writeable.
            struct object *object;
            if ((object = (struct object *)get_meta(Lua, ValueIndex, "Fluid.obj"))) {
               OBJECTPTR ptr_obj;
               if (object->prvObject) {
                  return SetPointer(src, field->FieldID, object->prvObject);
               }
               else if ((ptr_obj = (OBJECTPTR)access_object(object))) {
                  ERROR error = SetPointer(src, field->FieldID, object->prvObject);
                  release_object(object);
                  return error;
               }
               else return ERR_Failed;
            }
            else return SetPointer(src, field->FieldID, NULL);
         }
         else if (type IS LUA_TSTRING) {
            return SetString(src, field->FieldID, lua_tostring(Lua, ValueIndex));
         }
         else if (type IS LUA_TNUMBER) {
            if (field->Flags & FD_STRING) {
               return SetString(obj, field->FieldID, lua_tostring(Lua, ValueIndex));
            }
            else if (lua_tointeger(Lua, ValueIndex) IS 0) {
               // Setting pointer fields with numbers is only allowed if that number evaluates to zero (NULL)
               return SetLong(obj, field->FieldID, 0);
            }
            else return ERR_FieldTypeMismatch;
         }
         else if ((memory = (struct memory *)get_meta(Lua, ValueIndex, "Fluid.mem"))) {
            return SetPointer(obj, field->FieldID, memory->Memory);
         }
         else if ((fstruct = (struct fstruct *)get_meta(Lua, ValueIndex, "Fluid.struct"))) {
            return SetPointer(obj, field->FieldID, fstruct->Data);
         }
         else if (type IS LUA_TNIL) {
            return SetPointer(obj, field->FieldID, NULL);
         }
         else return ERR_FieldTypeMismatch;
      }
      else switch(type) {
         case LUA_TNUMBER:
            return SetDouble(src, field->FieldID, lua_tonumber(Lua, ValueIndex));

         case LUA_TBOOLEAN:
            return SetLong(src, field->FieldID, lua_toboolean(Lua, ValueIndex));

         case LUA_TNIL: // Setting a field with nil does nothing.  Use zero to be explicit.
            return ERR_Okay;

         case LUA_TUSERDATA: {
            struct object *object;
            if ((object = (struct object *)get_meta(Lua, ValueIndex, "Fluid.obj"))) {
               return SetLong(src, field->FieldID, object->ObjectID);
            }
            return ERR_FieldTypeMismatch;
         }

         case LUA_TSTRING:
            return SetFieldEval(obj, FName, lua_tostring(Lua, ValueIndex));

         case LUA_TTABLE:
         case LUA_TLIGHTUSERDATA:
         case LUA_TFUNCTION:
         case LUA_TTHREAD:
         default:
            return ERR_FieldTypeMismatch;
      }
   }
   else {
      // Default to setting a custom variable rather than throwing an error - primarily for legacy reasons.
      CSTRING vstr = lua_tostring(Lua, ValueIndex);
      if (vstr) {
         log.msg("Field '%s' is not in class '%s' - defaulting to custom variable. [DEPRECATED]", FName, src->Class->ClassName);
         return SetFieldEval(obj, FName, vstr);
      }
      else return ERR_UnsupportedField;
   }
}
