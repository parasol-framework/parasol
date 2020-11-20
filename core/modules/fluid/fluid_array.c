/*****************************************************************************

This code can be utilised internally for returning C arrays back to Lua.  Its use is required in cases where an
array needs to represent a fixed region in memory.  Writing values to the array will result in an update to that area
as opposed to a buffered region in Lua's memory space.  Arrays must be sized, so cannot be unbounded.  Null-terminated
arrays are permitted as their size can be computed at the time of creation.

If an array of values is read-only, please use standard Lua arrays rather than this interface.

Example use cases: Arrays in C structs and those returned by module functions.

In the case of Parasol classes that declare array fields, this interface cannot be used due to the potential for
mishap, so standard Lua tables are allocated for that use case.

To reference fields in the array:

   myarray[20] = "XYZ"
   var = myarray[20]

It is possible to create strings from any area of a byte array:

   val = myarray.getstring(10,10)

To convert the C array values to a Lua table:

   local luaArray = myarray.table()

*****************************************************************************/

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lj_obj.h"
#include "hashes.h"
#include "defs.h"

static int array_copy(lua_State *);

/*****************************************************************************
** If List is NULL and Total > 0, the list will be allocated.
**
** Note: It is okay for an array to be created that contains no elements.  For structs, the StructName will be
** referenced and the pointers stored, but no struct objects will be created until the array indexes are read.
**
** FieldType  - An FD describing the array elements.  Use FD_READ if the array is read-only.
** StructName - For struct arrays, a registered struct name is required.  Can be in the format 'StructName:ArgName'
** List       - Pointer to the array data.
** Total      - The total number of elements.  If -1, the List will be tallied until an empty value is reached.
** Cache      - Set to TRUE if the array should be cached (important if the List is temporary data).
*/

void make_array(lua_State *Lua, LONG FieldType, CSTRING StructName, APTR *List, LONG Total, BYTE Cache)
{
   objScript *Self = Lua->Script;
   struct prvFluid *prv = Self->Head.ChildPrivate;

   FieldType &= (FD_DOUBLE|FD_LARGE|FD_FLOAT|FD_POINTER|FD_STRING|FD_STRUCT|FD_FLOAT|FD_LONG|FD_WORD|FD_BYTE);

   if (FieldType & FD_STRING) FieldType = FD_STRING; // Eliminate confusion when FD_STRING|FD_POINTER might be combined

   FMSG("~make_array()","Content: %p, Type: $%.8x, Struct: %s, Total: %d, Cache: %d", List, FieldType, StructName, Total, Cache);

   // Find the struct definition if this array references one.  Note that struct arrays can be pointer based if
   // FD_POINTER is used, otherwise it is presumed that they are sequential.

   struct structentry *sdef = NULL;
   if (FieldType & FD_STRUCT) {
      if (!StructName) { lua_pushnil(Lua); LOGRETURN(); return; }
      {
         char struct_name[60];
         LONG i;
         for (i=0; (i < sizeof(struct_name)-1) AND (StructName[i] != ':'); i++) struct_name[i] = StructName[i];
         struct_name[i] = 0;

         if (VarGet(prv->Structs, struct_name, &sdef, NULL) != ERR_Okay) {
            LogF("@make_array","Struct '%s' is not registered.", StructName);
            lua_pushnil(Lua);
            LOGRETURN();
            return;
         }
      }
   }

   LONG type_size = 0;
   if (FieldType & FD_LONG)        type_size = sizeof(LONG);
   else if (FieldType & FD_WORD)   type_size = sizeof(WORD);
   else if (FieldType & FD_BYTE)   type_size = sizeof(BYTE);
   else if (FieldType & FD_FLOAT)  type_size = sizeof(FLOAT);
   else if (FieldType & FD_DOUBLE) type_size = sizeof(DOUBLE);
   else if (FieldType & FD_LARGE)  type_size = sizeof(LARGE);
   else if (FieldType & (FD_STRING|FD_POINTER)) type_size = sizeof(APTR);
   else if (FieldType & FD_STRUCT) type_size = sdef->Size; // The length of sequential structs cannot be calculated.
   else {
      lua_pushnil(Lua);
      LOGRETURN();
      return;
   }

   // Calculate the array length if the total is unspecified.

   if ((List) AND (Total < 0)) {
      if (FieldType & FD_LONG)        for (Total=0; ((LONG *)List)[Total]; Total++);
      else if (FieldType & FD_WORD)   for (Total=0; ((WORD *)List)[Total]; Total++);
      else if (FieldType & FD_BYTE)   for (Total=0; ((BYTE *)List)[Total]; Total++);
      else if (FieldType & FD_FLOAT)  for (Total=0; ((FLOAT *)List)[Total]; Total++);
      else if (FieldType & FD_DOUBLE) for (Total=0; ((DOUBLE *)List)[Total]; Total++);
      else if (FieldType & FD_LARGE)  for (Total=0; ((LARGE *)List)[Total]; Total++);
      else if (FieldType & (FD_STRING|FD_POINTER)) for (Total=0; ((APTR *)List)[Total]; Total++);
      else if (FieldType & FD_STRUCT) Total = -1; // The length of sequential structs cannot be calculated.
   }

   LONG array_size = 0;  // Size of the array in bytes, not including any cached content.
   LONG cache_size = 0;  // Size of the array in bytes, plus additional cached content.

   UBYTE alloc = FALSE;
   if (Total > 0) {
      cache_size = Total * type_size;
      array_size = Total * type_size;

      // If no list is provided but the total elements > 0, then the list must be allocated automatically.

      if (!List) {
         Cache = FALSE;
         alloc = TRUE;
         if (AllocMemory(array_size, MEM_DATA, &List, NULL) != ERR_Okay) {
            lua_pushnil(Lua);
            LOGRETURN();
            return;
         }
      }
   }

   if ((Cache) AND (List) AND (Total > 0)) {
      if (FieldType & FD_STRING) {
         LONG i;
         for (i=0; i < Total; i++) cache_size += StrLength(List[i]) + 1;
      }
   }

   LONG struct_nsize = 0;
   if (StructName) struct_nsize = StrLength(StructName) + 1;

   struct array *array;
   if ((array = (struct array *)lua_newuserdata(Lua, sizeof(struct array) + cache_size + struct_nsize))) {
      array->Total     = Total;
      array->Type      = FieldType;
      array->ArraySize = array_size;
      array->StructDef = sdef;
      array->TypeSize  = type_size;
      array->ReadOnly  = (FieldType & FD_READ) ? TRUE : FALSE;

      if ((Cache) AND (List) AND (Total > 0)) {
         array->ptrPointer = (APTR *)(array + 1);

         if (FieldType & FD_STRING) {
            CopyMemory(List, array->ptrPointer, Total * sizeof(APTR));

            STRING str = (STRING)(array->ptrString + Total);
            LONG i;
            for (i=0; i < Total; i++) {
               array->ptrString[i] = str;
               str += StrCopy(List[i], str, COPY_ALL) + 1;
            }
         }
         else CopyMemory(List, array->ptrPointer, cache_size);

         if (alloc) FreeResource(List);
         array->Allocated = FALSE;
      }
      else {
         array->ptrPointer = List;
         array->Allocated = alloc;
      }

      luaL_getmetatable(Lua, "Fluid.array");
      lua_setmetatable(Lua, -2);
      MSG("Succeeded in creating array.");
      // The array object will be returned on the stack due to the lua_newuserdata() call
   }
   else {
      if (alloc) FreeResource(List);
      lua_pushnil(Lua); // Must return a value even if it is nil
   }
   LOGRETURN();
}

/*****************************************************************************
** Usage: array = array.new(InitialSize, Type)
**
** Creates a new array of the given size and value type.
**
** var = array.new(100, "integer")
**
** You can convert a string into a byte array to simplify string parsing as follows:
**
** var = array.new("mystring", "bytestring")
*/

static int array_new(lua_State *Lua)
{
   FMSG("array_new()","");

   CSTRING type;
   if ((type = lua_tostring(Lua, 2))) {
      LONG total;
      if (!StrMatch("bytestring", type)) {
         // Represent a string as an array of bytes

         CSTRING str;
         size_t len;
         if ((str = lua_tolstring(Lua, 1, &len))) {
            MSG("Generating byte array from string of length %d: %.30s", (int)len, str);

            struct array *array;
            if ((array = (struct array *)lua_newuserdata(Lua, sizeof(struct array) + len + 1))) {
               array->Total   = len;
               array->Type    = FD_BYTE;
               array->ptrByte = (BYTE *)(array + 1);
               CopyMemory(str, array->ptrByte, len + 1);

               luaL_getmetatable(Lua, "Fluid.array");
               lua_setmetatable(Lua, -2);
               return 1; // userdata reference is already on the stack
            }
            else {
               lua_pushnil(Lua);
               return 1;
            }
         }
         else luaL_argerror(Lua, 1, "A string must be provided if using the 'bytestring' array type.");
      }
      else if ((total = lua_tointeger(Lua, 1))) {
         LONG fieldtype;
         switch (StrHash(type, 0)) {
            case HASH_LONG:
            case HASH_INTEGER: fieldtype = FD_LONG; break;
            case HASH_STRING:  fieldtype = FD_STRING; break;
            case HASH_SHORT:
            case HASH_WORD:    fieldtype = FD_WORD; break;
            case HASH_BYTE:    fieldtype = FD_BYTE; break;
            case HASH_LARGE:   fieldtype = FD_LARGE; break;
            case HASH_DOUBLE:  fieldtype = FD_DOUBLE; break;
            case HASH_FLOAT:   fieldtype = FD_FLOAT; break;
            case HASH_PTR:
            case HASH_POINTER: fieldtype = FD_POINTER; break;
            default:
               luaL_argerror(Lua, 2, "Unrecognised type specified.");
               return 0;
         }

         make_array(Lua, fieldtype, NULL, NULL, total, TRUE);
         return 1;
      }
      else luaL_argerror(Lua, 1, "Array type required.");
   }
   else luaL_argerror(Lua, 2, "Array value type requird.");

   return 0;
}

/*****************************************************************************
** Usage: string = array:getstring(start, len)
**
** Creates a string from a byte array.  If len is nil, the entire buffer from the starting index up to the end of the
** byte array is returned.
*/

static int array_getstring(lua_State *Lua)
{
   struct array *array;
   if (!(array = get_meta(Lua, lua_upvalueindex(1), "Fluid.array"))) {
      luaL_error(Lua, "Expected array in upvalue.");
      return 0;
   }

   if (array->Type != FD_BYTE) {
      luaL_error(Lua, "getstring() only works with byte arrays.");
      return 0;
   }

   LONG len, start;
   if (lua_isnil(Lua, 1)) start = 0;
   else {
      start = lua_tointeger(Lua, 1);
      if ((start < 0) OR (start >= array->Total)) {
         luaL_argerror(Lua, 1, "Invalid starting index.");
         return 0;
      }
   }

   if (lua_isnumber(Lua,2)) {
      len = lua_tointeger(Lua, 2);
      if ((len < 1) OR (start+len > array->Total)) {
         luaL_error(Lua, "Invalid length: Index %d < %d < %d", start, start+len, array->Total);
         return 0;
      }
   }
   else len = array->Total - start;

   if (len < 1) lua_pushstring(Lua, "");
   else lua_pushlstring(Lua, array->ptrByte + start, len);

   return 1;
}

/*****************************************************************************
** Internal: Array index call
**
** Any Read accesses to the object will pass through here.
*/

static int array_get(lua_State *Lua)
{
   struct array *array;
   if ((array = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array"))) {

      CSTRING field;
      if (lua_type(Lua, 2) IS LUA_TNUMBER) { // Array reference discovered, e.g. myarray[18]
         LONG index = lua_tonumber(Lua, 2);

         FMSG("~array_get()","Index: %d", index);

            // Check that the index is legal

            if ((index < 1) OR (index > array->Total)) {
               LOGRETURN();
               luaL_error(Lua, "Invalid array index: 1 < %d <= %d", index, array->Total);
               return 0;
            }

            MSG("array.index(%d)", index);

            index--; // Convert Lua index to C index
            switch(array->Type & (FD_DOUBLE|FD_LARGE|FD_FLOAT|FD_POINTER|FD_STRUCT|FD_STRING|FD_LONG|FD_WORD|FD_BYTE)) {
               case FD_STRUCT:  if (struct_to_table(Lua, NULL, array->StructDef, array->ptrPointer[index]) != ERR_Okay) lua_pushnil(Lua); break;
               case FD_STRING:  lua_pushstring(Lua, array->ptrString[index]); break;
               case FD_POINTER: lua_pushlightuserdata(Lua, array->ptrPointer[index]); break;
               case FD_FLOAT:   lua_pushnumber(Lua, array->ptrFloat[index]); break;
               case FD_DOUBLE:  lua_pushnumber(Lua, array->ptrDouble[index]); break;
               case FD_LARGE:   lua_pushnumber(Lua, array->ptrLarge[index]); break;
               case FD_LONG:    lua_pushinteger(Lua, array->ptrLong[index]); break;
               case FD_WORD:    lua_pushinteger(Lua, array->ptrWord[index]); break;
               case FD_BYTE:    lua_pushinteger(Lua, array->ptrByte[index]); break;
               default:
                  LogErrorMsg("Unsupported array type $%.8x", array->Type);
                  lua_pushnil(Lua);
                  break;
            }

         LOGRETURN();
         return 1;
      }
      else if ((field = luaL_checkstring(Lua, 2))) {
         FMSG("~array_get()","Field: %s", field);

         if (!StrMatch("table", field)) {
            // Convert the array to a standard Lua table.

            lua_createtable(Lua, array->Total, 0); // Create a new table on the stack.
            LONG i;
            switch(array->Type & (FD_DOUBLE|FD_LARGE|FD_FLOAT|FD_POINTER|FD_STRUCT|FD_STRING|FD_LONG|FD_WORD|FD_BYTE)) {
               case FD_STRUCT:  {
                  struct references *ref;
                  if ((ref = alloc_references())) {
                     for (i=0; i < array->Total; i++) {
                        lua_pushinteger(Lua, i);
                        if (struct_to_table(Lua, ref, array->StructDef, array->ptrPointer[i]) != ERR_Okay) lua_pushnil(Lua);
                        lua_settable(Lua, -3);
                     }
                     free_references(Lua, ref);
                  }
                  break;
               }
               case FD_STRING:  for (i=0; i < array->Total; i++) { lua_pushinteger(Lua, i); lua_pushstring(Lua, array->ptrString[i]); lua_settable(Lua, -3); } break;
               case FD_POINTER: for (i=0; i < array->Total; i++) { lua_pushinteger(Lua, i); lua_pushlightuserdata(Lua, array->ptrPointer[i]); lua_settable(Lua, -3); } break;
               case FD_FLOAT:   for (i=0; i < array->Total; i++) { lua_pushinteger(Lua, i); lua_pushnumber(Lua, array->ptrFloat[i]); lua_settable(Lua, -3); } break;
               case FD_DOUBLE:  for (i=0; i < array->Total; i++) { lua_pushinteger(Lua, i); lua_pushnumber(Lua, array->ptrDouble[i]); lua_settable(Lua, -3); } break;
               case FD_LARGE:   for (i=0; i < array->Total; i++) { lua_pushinteger(Lua, i); lua_pushnumber(Lua, array->ptrLarge[i]); lua_settable(Lua, -3); } break;
               case FD_LONG:    for (i=0; i < array->Total; i++) { lua_pushinteger(Lua, i); lua_pushinteger(Lua, array->ptrLong[i]); lua_settable(Lua, -3); } break;
               case FD_WORD:    for (i=0; i < array->Total; i++) { lua_pushinteger(Lua, i); lua_pushinteger(Lua, array->ptrWord[i]); lua_settable(Lua, -3); } break;
               case FD_BYTE:    for (i=0; i < array->Total; i++) { lua_pushinteger(Lua, i); lua_pushinteger(Lua, array->ptrByte[i]); lua_settable(Lua, -3); } break;
            }

            LOGRETURN();
            return 1;
         }
         else if (!StrMatch("getstring", field)) {
            lua_pushvalue(Lua, 1); // Arg1: Duplicate the object reference
            lua_pushcclosure(Lua, array_getstring, 1);
            LOGRETURN();
            return 1;
         }
         else if (!StrMatch("copy", field)) {
            lua_pushvalue(Lua, 1); // Arg1: Duplicate the object reference
            lua_pushcclosure(Lua, array_copy, 1);
            LOGRETURN();
            return 1;
         }

         LOGRETURN();
         luaL_error(Lua, "Reference to %s not recognised.", field);
      }
      else luaL_error(Lua, "No field reference provided");
   }
   else luaL_error(Lua, "Invalid caller, expected Fluid.array.");

   return 0;
}

/*****************************************************************************
** Usage: array.field = newvalue
*/

static int array_set(lua_State *Lua)
{
   struct array *array;
   if ((array = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array"))) {
      if (array->ReadOnly) { luaL_error(Lua, "Array is read-only."); return 0; }

      if (lua_type(Lua, 2) IS LUA_TNUMBER) { // Array index
         LONG index = lua_tonumber(Lua, 2);
         if ((index < 1) OR (index > array->Total)) {
            LOGRETURN();
            luaL_error(Lua, "Invalid array index: 1 < %d <= %d", index, array->Total);
            return 0;
         }

         index--; // Convert Lua index to C index

         if (array->Type & FD_STRUCT) {
            if (array->Type & FD_POINTER) { // Array of struct pointers
               luaL_error(Lua, "Writing to struct pointer arrays not yet supported.");
            }
            else { // Array of sequential structs
               luaL_error(Lua, "Writing to struct arrays not yet supported.");
            }
         }
         else if (array->Type & FD_STRING)  {
            //The code below would need a Lua reference to the string, or clone of it.
            //array->ptrString[index]  = (STRING)lua_tostring(Lua, 3);
            luaL_error(Lua, "Writing to string arrays is not yet supported.");
         }
         else if (array->Type & FD_POINTER) {
            // Writing to pointer arrays is too dangerous
            //array->ptrPointer[index] = lua_touserdata(Lua, 3);
            luaL_error(Lua, "Writing to pointer arrays is not supported.");
         }
         else if (array->Type & FD_FLOAT)   array->ptrFloat[index]   = lua_tonumber(Lua, 3);
         else if (array->Type & FD_DOUBLE)  array->ptrDouble[index]  = lua_tonumber(Lua, 3);
         else if (array->Type & FD_LARGE)   array->ptrLarge[index]   = lua_tointeger(Lua, 3);
         else if (array->Type & FD_LONG)    array->ptrLong[index]    = lua_tointeger(Lua, 3);
         else if (array->Type & FD_WORD)    array->ptrWord[index]    = lua_tointeger(Lua, 3);
         else if (array->Type & FD_BYTE)    array->ptrByte[index]    = lua_tointeger(Lua, 3);
         else luaL_error(Lua, "Unsupported array type $%.8x", array->Type);
      }
      else luaL_error(Lua, "Array index expected in 2nd argument.");
   }
   else luaL_error(Lua, "Invalid caller, expected Fluid.array.");

   return 0;
}

//****************************************************************************
// Usage: string = array:copy(source, [DestIndex], [Total])
//
// Copies a string or data sequence to the memory block.

static int array_copy(lua_State *Lua)
{
   struct array *array;
   if (!(array = get_meta(Lua, lua_upvalueindex(1), "Fluid.array"))) {
      luaL_error(Lua, "Expected array in upvalue.");
      return 0;
   }

   if (array->ReadOnly) { luaL_error(Lua, "Array is read-only."); return 0; }

   LONG to_index = 1;
   if (!lua_isnumber(Lua, 2)) {
      to_index = lua_tointeger(Lua, 2);
      if (to_index < 1) {
         luaL_argerror(Lua, 2, "Invalid destination index.");
         return 0;
      }
   }

   size_t req_total = -1;
   if (lua_isnumber(Lua, 3)) {
      req_total = lua_tointeger(Lua, 3);
      if (req_total < 1) { luaL_argerror(Lua, 3, "Invalid total."); return 0; }
   }

   struct array *src_array;
   size_t src_total;
   const void *src;
   LONG src_typesize;
   if ((src = luaL_checklstring(Lua, 1, &src_total))) {
      src_typesize = 1;
      if (req_total > src_total) {
         luaL_argerror(Lua, 3, "Invalid total.");
         return 0;
      }
   }
   else if ((src_array = get_meta(Lua, 1, "Fluid.array"))) {
      src_typesize = src_array->TypeSize;
      src_total    = src_array->Total;
   }
   else if (lua_istable(Lua, 1)) {
      luaL_argerror(Lua, 1, "Tables not supported yet.");
      return 0;
   }
   else { luaL_argerror(Lua, 1, "String or array expected."); return 0; }

   to_index--; // Lua index to C index
   if (to_index + src_total > array->Total) {
      luaL_error(Lua, "Invalid index or total (%d+%d > %d).", to_index, src_total, array->Total);
      return 0;
   }

   if (src_typesize IS array->TypeSize) {
      CopyMemory(src, array->ptrPointer + (to_index * src_typesize), req_total * src_typesize);
   }
   else {
      LONG i;
      for (i=0; i < req_total; i++) {
         LARGE s;
         switch (src_typesize) {
            case 1: s = ((BYTE *)src)[0]; break;
            case 2: s = ((WORD *)src)[0]; break;
            case 4: s = ((LONG *)src)[0]; break;
            case 8: s = ((LARGE *)src)[0]; break;
            default: s = 0; break;
         }

         switch (array->TypeSize) {
            case 1: ((BYTE *)src)[0]  = s; break;
            case 2: ((WORD *)src)[0]  = s; break;
            case 4: ((LONG *)src)[0]  = s; break;
            case 8: ((LARGE *)src)[0] = s; break;
         }

         src += src_typesize;
      }
   }

   return 0;
}

//****************************************************************************
// Garbage collecter.

static int array_destruct(lua_State *Lua)
{
   struct array *array;

   if ((array = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array"))) {
      if (array->Allocated) {
         if (FreeResource(array->ptrPointer)) LogF("@array_destruct","Data address %p is invalid.", array->ptrPointer);
         array->ptrPointer = NULL;
         array->Allocated = FALSE;
         array->Total = 0;
         array->Type = 0;
      }
   }

   return 0;
}

//****************************************************************************
// Array length.

static int array_len(lua_State *Lua)
{
   struct array *array;
   if ((array = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array"))) {
      lua_pushinteger(Lua, array->Total);
   }
   else lua_pushinteger(Lua, 0);

   return 1;
}

//****************************************************************************
// Register the array interface.

void register_array_class(lua_State *Lua)
{
   static const struct luaL_reg functions[] = {
      { "new",  array_new },
      { NULL, NULL }
   };

   static const struct luaL_reg methods[] = {
      { "__index",    array_get },
      { "__newindex", array_set },
      { "__len",      array_len },
      { "__gc",       array_destruct },
      { NULL, NULL }
   };

   MSG("Registering array interface.");

   luaL_newmetatable(Lua, "Fluid.array");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable
   luaL_openlib(Lua, NULL, methods, 0);

   luaL_openlib(Lua, "array", functions, 0);
}
