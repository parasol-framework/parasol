/*********************************************************************************************************************

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

*********************************************************************************************************************/

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <parasol/strings.hpp>

extern "C" {
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

static int array_copy(lua_State *);

/*********************************************************************************************************************
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

void make_array(lua_State *Lua, LONG FieldType, CSTRING StructName, APTR *List, LONG Total, bool Cache)
{
   pf::Log log(__FUNCTION__);
   auto Self = Lua->Script;
   auto prv = (prvFluid *)Self->ChildPrivate;

   FieldType &= (FD_DOUBLE|FD_INT64|FD_FLOAT|FD_POINTER|FD_STRING|FD_STRUCT|FD_FLOAT|FD_INT|FD_WORD|FD_BYTE|FD_CPP);

   if (FieldType & FD_STRING) FieldType &= FD_STRING|FD_CPP; // Eliminate confusion when FD_STRING|FD_POINTER might be combined

   log.traceBranch("Content: %p, Type: $%.8x, Struct: %s, Total: %d, Cache: %d", List, FieldType, StructName, Total, Cache);

   // Find the struct definition if this array references one.  Note that struct arrays can be pointer based if
   // FD_POINTER is used, otherwise it is presumed that they are sequential.

   struct_record *sdef = NULL;
   if (FieldType & FD_STRUCT) {
      if (!StructName) { lua_pushnil(Lua); return; }

      auto name = struct_name(StructName);
      if (prv->Structs.contains(name)) {
         sdef = &prv->Structs[name];
      }
      else {
         log.warning("Struct '%s' is not registered.", StructName);
         lua_pushnil(Lua);
         return;
      }
   }

   LONG type_size = 0;
   if (FieldType & FD_INT)        type_size = sizeof(LONG);
   else if (FieldType & FD_WORD)   type_size = sizeof(WORD);
   else if (FieldType & FD_BYTE)   type_size = sizeof(BYTE);
   else if (FieldType & FD_FLOAT)  type_size = sizeof(FLOAT);
   else if (FieldType & FD_DOUBLE) type_size = sizeof(DOUBLE);
   else if (FieldType & FD_INT64)  type_size = sizeof(LARGE);
   else if (FieldType & FD_STRING) {
      if (FieldType & FD_CPP) type_size = sizeof(std::string);
      else type_size = sizeof(APTR);
   }
   else if (FieldType & FD_POINTER) type_size = sizeof(APTR);
   else if (FieldType & FD_STRUCT) type_size = sdef->Size; // The length of sequential structs cannot be calculated.
   else {
      lua_pushnil(Lua);
      return;
   }

   // Calculate the array length if the total is unspecified.

   if ((List) and (Total < 0)) {
      if (FieldType & FD_INT)        for (Total=0; ((LONG *)List)[Total]; Total++);
      else if (FieldType & FD_WORD)   for (Total=0; ((WORD *)List)[Total]; Total++);
      else if (FieldType & FD_BYTE)   for (Total=0; ((BYTE *)List)[Total]; Total++);
      else if (FieldType & FD_FLOAT)  for (Total=0; ((FLOAT *)List)[Total]; Total++);
      else if (FieldType & FD_DOUBLE) for (Total=0; ((DOUBLE *)List)[Total]; Total++);
      else if (FieldType & FD_INT64)  for (Total=0; ((LARGE *)List)[Total]; Total++);
      else if (FieldType & FD_STRING) {
         if (FieldType & FD_CPP) { // Null-terminated std::string lists aren't permitted.
            lua_pushnil(Lua);
            return;
         }
         else for (Total=0; ((CSTRING *)List)[Total]; Total++);
      }
      else if (FieldType & FD_POINTER) for (Total=0; ((APTR *)List)[Total]; Total++);
      else if (FieldType & FD_STRUCT) Total = -1; // The length of sequential structs cannot be calculated.
   }

   LONG array_size = 0;  // Size of the array in bytes, not including any cached content.
   LONG cache_size = 0;  // Size of the array in bytes, plus additional cached content.

   bool alloc = false;
   if (Total > 0) {
      cache_size = Total * type_size;
      array_size = Total * type_size;

      // If no list is provided but the total elements > 0, then the list must be allocated automatically.

      if (!List) {
         Cache = false;
         alloc = true;
         if (AllocMemory(array_size, MEM::DATA, &List) != ERR::Okay) {
            lua_pushnil(Lua);
            return;
         }
      }
   }

   if ((Cache) and (List) and (Total > 0)) {
      if (FieldType & FD_STRING) {
         if (FieldType & FD_CPP) {
            for (LONG i=0; i < Total; i++) cache_size += ((std::string *)List)[i].size() + 1;
         }
         else for (LONG i=0; i < Total; i++) cache_size += strlen((CSTRING)List[i]) + 1;
      }
   }

   LONG struct_nsize = 0;
   if (StructName) struct_nsize = strlen(StructName) + 1;

   if (auto a = (struct array *)lua_newuserdata(Lua, sizeof(struct array) + cache_size + struct_nsize)) {
      a->Total       = Total;
      a->Type        = FieldType;
      a->ArraySize   = array_size;
      a->StructDef   = sdef;
      a->TypeSize    = type_size;
      a->AlignedSize = ALIGN64(type_size);
      a->ReadOnly    = (FieldType & FD_READ) ? true : false;

      if ((Cache) and (List) and (Total > 0)) {
         a->ptrPointer = (APTR *)(a + 1);

         if (FieldType & FD_STRING) {
            if (FieldType & FD_CPP) {
               auto str = (STRING)(a->ptrString + Total);
               for (LONG i=0; i < Total; i++) {
                  a->ptrString[i] = str;
                  str += strcopy(((std::string *)List)[i].c_str(), str) + 1;
               }
            }
            else {
               auto str = (STRING)(a->ptrString + Total);
               for (LONG i=0; i < Total; i++) {
                  a->ptrString[i] = str;
                  str += strcopy((CSTRING)List[i], str) + 1;
               }
            }
         }
         else copymem(List, a->ptrPointer, cache_size);

         if (alloc) FreeResource(List);
         a->Allocated = FALSE;
      }
      else {
         a->ptrPointer = List;
         a->Allocated = alloc;
      }

      luaL_getmetatable(Lua, "Fluid.array");
      lua_setmetatable(Lua, -2);
      // The array object will be returned on the stack due to the lua_newuserdata() call
   }
   else {
      if (alloc) FreeResource(List);
      lua_pushnil(Lua); // Must return a value even if it is nil
   }
}

//********************************************************************************************************************
// Usage: array = array.new(InitialSize, Type)
//
// Creates a new array of the given size and value type.
//
// var = array.new(100, "integer")
//
// You can convert a string into a byte array to simplify string parsing as follows:
//
// var = array.new("mystring", "bytestring")

static int array_new(lua_State *Lua)
{
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   if (auto type = lua_tostring(Lua, 2)) {
      pf::Log log(__FUNCTION__);

      log.trace("");
      if (iequals("bytestring", type)) { // Represent a string as an array of bytes
         size_t len;
         if (auto str = lua_tolstring(Lua, 1, &len)) {
            log.trace("Generating byte array from string of length %d: %.30s", (int)len, str);

            if (auto a = (struct array *)lua_newuserdata(Lua, sizeof(struct array) + len + 1)) {
               a->Total   = len;
               a->Type    = FD_BYTE;
               a->ptrByte = (UBYTE *)(a + 1);
               copymem(str, a->ptrByte, len + 1);

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
      else if (auto total = lua_tointeger(Lua, 1)) {
         LONG fieldtype;
         CSTRING s_name = NULL;
         switch (strihash(type)) {
            case HASH_LONG:
            case HASH_INTEGER: fieldtype = FD_INT; break;
            case HASH_STRING:  fieldtype = FD_STRING; break;
            case HASH_SHORT:
            case HASH_WORD:    fieldtype = FD_WORD; break;
            case HASH_BYTE:    fieldtype = FD_BYTE; break;
            case HASH_LARGE:   fieldtype = FD_INT64; break;
            case HASH_DOUBLE:  fieldtype = FD_DOUBLE; break;
            case HASH_FLOAT:   fieldtype = FD_FLOAT; break;
            case HASH_PTR:
            case HASH_POINTER: fieldtype = FD_POINTER; break;
            default:
               // Check if the type refers to a struct
               auto s = struct_name(type);
               if (prv->Structs.contains(s)) {
                  fieldtype = FD_STRUCT;
                  s_name    = type;
               }
               else {
                  luaL_error(Lua, "Unrecognised type '%s' specified.", type);
                  return 0;
               }
         }

         make_array(Lua, fieldtype, s_name, NULL, total, true);
         return 1;
      }
      else luaL_argerror(Lua, 1, "Array type required.");
   }
   else luaL_argerror(Lua, 2, "Array value type requird.");

   return 0;
}

//********************************************************************************************************************
// Usage: string = array.getstring(start, len)
//
// Creates a string from a byte array.  If len is nil, the entire buffer from the starting index up to the end of the
// byte array is returned.

static int array_getstring(lua_State *Lua)
{
   auto a = (struct array *)get_meta(Lua, lua_upvalueindex(1), "Fluid.array");
   if (!a) {
      luaL_error(Lua, "Expected array.");
      return 0;
   }

   if (a->Type != FD_BYTE) {
      luaL_error(Lua, "getstring() only works with byte arrays.");
      return 0;
   }

   LONG len, start;
   if (lua_isnil(Lua, 1)) start = 0;
   else {
      start = lua_tointeger(Lua, 1);
      if ((start < 0) or (start >= a->Total)) {
         luaL_argerror(Lua, 1, "Invalid starting index.");
         return 0;
      }
   }

   if (lua_isnumber(Lua,2)) {
      len = lua_tointeger(Lua, 2);
      if ((len < 0) or (start+len > a->Total)) {
         luaL_error(Lua, "Invalid length: Index %d < %d < %d", start, start+len, a->Total);
         return 0;
      }
   }
   else len = a->Total - start;

   if (len < 1) lua_pushstring(Lua, "");
   else lua_pushlstring(Lua, (CSTRING)a->ptrByte + start, len);

   return 1;
}

//********************************************************************************************************************
// Any Read accesses will pass through here.

static int array_get(lua_State *Lua)
{
   if (auto a = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array")) {
      pf::Log log(__FUNCTION__);
      CSTRING field;
      if (lua_type(Lua, 2) IS LUA_TNUMBER) { // Array reference discovered, e.g. myarray[18]
         LONG index = lua_tointeger(Lua, 2);

         if ((index < 1) or (index > a->Total)) {
            luaL_error(Lua, "Invalid array index: 1 < %d <= %d", index, a->Total);
            return 0;
         }

         log.trace("array.index(%d)", index);

         index--; // Convert Lua index to C index
         switch(a->Type & (FD_DOUBLE|FD_INT64|FD_FLOAT|FD_POINTER|FD_STRUCT|FD_STRING|FD_INT|FD_WORD|FD_BYTE)) {
            case FD_STRUCT: {
               // Arrays of structs are presumed to be in sequence, as opposed to an array of pointers to structs.
               std::vector<lua_ref> ref;
               if (struct_to_table(Lua, ref, a->StructDef[0], a->ptrByte + (index * a->AlignedSize)) != ERR::Okay) {
                  lua_pushnil(Lua);
               }
               break;
            }
            case FD_STRING:  lua_pushstring(Lua, a->ptrString[index]); break;
            case FD_POINTER: lua_pushlightuserdata(Lua, a->ptrPointer[index]); break;
            case FD_FLOAT:   lua_pushnumber(Lua, a->ptrFloat[index]); break;
            case FD_DOUBLE:  lua_pushnumber(Lua, a->ptrDouble[index]); break;
            case FD_INT64:   lua_pushnumber(Lua, a->ptrLarge[index]); break;
            case FD_INT:     lua_pushinteger(Lua, a->ptrLong[index]); break;
            case FD_WORD:    lua_pushinteger(Lua, a->ptrWord[index]); break;
            case FD_BYTE:    lua_pushinteger(Lua, a->ptrByte[index]); break;
            default:
               log.warning("Unsupported array type $%.8x", a->Type);
               lua_pushnil(Lua);
               break;
         }

         return 1;
      }
      else if ((field = luaL_checkstring(Lua, 2))) {
         log.trace("Field: %s", field);

         if (std::string_view("table") == field) { // Convert the array to a standard Lua table.
            lua_createtable(Lua, a->Total, 0); // Create a new table on the stack.
            switch(a->Type & (FD_DOUBLE|FD_INT64|FD_FLOAT|FD_POINTER|FD_STRUCT|FD_STRING|FD_INT|FD_WORD|FD_BYTE)) {
               case FD_STRUCT:  {
                  std::vector<lua_ref> ref;
                  for (LONG i=0; i < a->Total; i++) {
                     lua_pushinteger(Lua, i);
                     if (struct_to_table(Lua, ref, a->StructDef[0], a->ptrPointer[i]) != ERR::Okay) lua_pushnil(Lua);
                     lua_settable(Lua, -3);
                  }
                  break;
               }
               case FD_STRING:  for (LONG i=0; i < a->Total; i++) { lua_pushinteger(Lua, i); lua_pushstring(Lua, a->ptrString[i]); lua_settable(Lua, -3); } break;
               case FD_POINTER: for (LONG i=0; i < a->Total; i++) { lua_pushinteger(Lua, i); lua_pushlightuserdata(Lua, a->ptrPointer[i]); lua_settable(Lua, -3); } break;
               case FD_FLOAT:   for (LONG i=0; i < a->Total; i++) { lua_pushinteger(Lua, i); lua_pushnumber(Lua, a->ptrFloat[i]); lua_settable(Lua, -3); } break;
               case FD_DOUBLE:  for (LONG i=0; i < a->Total; i++) { lua_pushinteger(Lua, i); lua_pushnumber(Lua, a->ptrDouble[i]); lua_settable(Lua, -3); } break;
               case FD_INT64:   for (LONG i=0; i < a->Total; i++) { lua_pushinteger(Lua, i); lua_pushnumber(Lua, a->ptrLarge[i]); lua_settable(Lua, -3); } break;
               case FD_INT:     for (LONG i=0; i < a->Total; i++) { lua_pushinteger(Lua, i); lua_pushinteger(Lua, a->ptrLong[i]); lua_settable(Lua, -3); } break;
               case FD_WORD:    for (LONG i=0; i < a->Total; i++) { lua_pushinteger(Lua, i); lua_pushinteger(Lua, a->ptrWord[i]); lua_settable(Lua, -3); } break;
               case FD_BYTE:    for (LONG i=0; i < a->Total; i++) { lua_pushinteger(Lua, i); lua_pushinteger(Lua, a->ptrByte[i]); lua_settable(Lua, -3); } break;
            }

            return 1;
         }
         else if (std::string_view("getstring") == field) {
            lua_pushvalue(Lua, 1); // Arg1: Duplicate the object reference
            lua_pushcclosure(Lua, array_getstring, 1);
            return 1;
         }
         else if (std::string_view("copy") == field) {
            lua_pushvalue(Lua, 1); // Arg1: Duplicate the object reference
            lua_pushcclosure(Lua, array_copy, 1);
            return 1;
         }

         luaL_error(Lua, "Reference to %s not recognised.", field);
      }
      else luaL_error(Lua, "No field reference provided");
   }
   else luaL_error(Lua, "Invalid caller, expected Fluid.array.");

   return 0;
}

/*********************************************************************************************************************
** Usage: array.field = newvalue
*/

static int array_set(lua_State *Lua)
{
   if (auto a = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array")) {
      if (a->ReadOnly) { luaL_error(Lua, "Array is read-only."); return 0; }

      if (lua_type(Lua, 2) IS LUA_TNUMBER) { // Array index
         LONG index = lua_tointeger(Lua, 2);
         if ((index < 1) or (index > a->Total)) {
            luaL_error(Lua, "Invalid array index: 1 < %d <= %d", index, a->Total);
            return 0;
         }

         index--; // Convert Lua index to C index

         if (a->Type & FD_STRUCT) {
            if (a->Type & FD_POINTER) { // Array of struct pointers
               luaL_error(Lua, "Writing to struct pointer arrays not yet supported.");
            }
            else { // Array of sequential structs
               luaL_error(Lua, "Writing to struct arrays not yet supported.");
            }
         }
         else if (a->Type & FD_STRING)  {
            //The code below would need a Lua reference to the string, or clone of it.
            //a->ptrString[index]  = (STRING)lua_tostring(Lua, 3);
            luaL_error(Lua, "Writing to string arrays is not yet supported.");
         }
         else if (a->Type & FD_POINTER) { // Writing to pointer arrays is too dangerous
            //a->ptrPointer[index] = lua_touserdata(Lua, 3);
            luaL_error(Lua, "Writing to pointer arrays is not supported.");
         }
         else if (a->Type & FD_FLOAT)  a->ptrFloat[index]  = lua_tonumber(Lua, 3);
         else if (a->Type & FD_DOUBLE) a->ptrDouble[index] = lua_tonumber(Lua, 3);
         else if (a->Type & FD_INT64)  a->ptrLarge[index]  = lua_tointeger(Lua, 3);
         else if (a->Type & FD_INT)    a->ptrLong[index]   = lua_tointeger(Lua, 3);
         else if (a->Type & FD_WORD)   a->ptrWord[index]   = lua_tointeger(Lua, 3);
         else if (a->Type & FD_BYTE)   a->ptrByte[index]   = lua_tointeger(Lua, 3);
         else luaL_error(Lua, "Unsupported array type $%.8x", a->Type);
      }
      else luaL_error(Lua, "Array index expected in 2nd argument.");
   }
   else luaL_error(Lua, "Invalid caller, expected Fluid.array.");

   return 0;
}

//********************************************************************************************************************
// Usage: array.copy(source, [DestIndex], [Total])
//
// Copies a string or data sequence to the array.

static int array_copy(lua_State *Lua)
{
   auto a = (struct array *)get_meta(Lua, lua_upvalueindex(1), "Fluid.array");

   if (!a) {
      luaL_error(Lua, "Expected array in upvalue.");
      return 0;
   }

   if (a->ReadOnly) { luaL_error(Lua, "Array is read-only."); return 0; }

   LONG to_index = 1;
   if (!lua_isnumber(Lua, 2)) {
      to_index = lua_tointeger(Lua, 2);
      if (to_index < 1) {
         luaL_argerror(Lua, 2, "Invalid destination index.");
         return 0;
      }
   }

   LONG req_total = -1;
   if (lua_isnumber(Lua, 3)) {
      req_total = lua_tointeger(Lua, 3);
      if (req_total < 1) { luaL_argerror(Lua, 3, "Invalid total."); return 0; }
   }

   size_t src_total;
   BYTE *src;
   LONG src_typesize;
   if ((src = (BYTE *)luaL_checklstring(Lua, 1, &src_total))) {
      src_typesize = 1;
      if ((size_t)req_total > src_total) {
         luaL_argerror(Lua, 3, "Invalid total.");
         return 0;
      }
   }
   else if (auto src_array = (struct array *)get_meta(Lua, 1, "Fluid.array")) {
      src_typesize = src_array->TypeSize;
      src_total    = src_array->Total;
   }
   else if (lua_istable(Lua, 1)) {
      luaL_argerror(Lua, 1, "Tables not supported yet.");
      return 0;
   }
   else { luaL_argerror(Lua, 1, "String or array expected."); return 0; }

   to_index--; // Lua index to C index
   if (to_index + src_total > (size_t)a->Total) {
      luaL_error(Lua, "Invalid index or total (%d+%d > %d).", to_index, src_total, a->Total);
      return 0;
   }

   if (src_typesize IS a->TypeSize) {
      copymem(src, a->ptrPointer + (to_index * src_typesize), req_total * src_typesize);
   }
   else {
      for (LONG i=0; i < req_total; i++) {
         LARGE s;
         switch (src_typesize) {
            case 1: s = ((BYTE *)src)[0]; break;
            case 2: s = ((WORD *)src)[0]; break;
            case 4: s = ((LONG *)src)[0]; break;
            case 8: s = ((LARGE *)src)[0]; break;
            default: s = 0; break;
         }

         switch (a->TypeSize) {
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

//********************************************************************************************************************
// Garbage collector.

static int array_destruct(lua_State *Lua)
{
   auto a = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array");

   if ((a) and (a->Allocated)) {
      FreeResource(a->ptrPointer);
      a->ptrPointer = NULL;
      a->Allocated = FALSE;
      a->Total = 0;
      a->Type = 0;
   }

   return 0;
}

//********************************************************************************************************************
// Array length.

static int array_len(lua_State *Lua)
{
   auto a = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array");
   if (a) lua_pushinteger(Lua, a->Total);
   else lua_pushinteger(Lua, 0);
   return 1;
}

//********************************************************************************************************************

static int array_tostring(lua_State *Lua)
{
   auto a = (struct array *)luaL_checkudata(Lua, 1, "Fluid.array");
   if ((a) and (a->Type IS FD_BYTE)) {
      lua_pushlstring(Lua, CSTRING(a->ptrByte), a->ArraySize);
   }
   else lua_pushstring(Lua, "[INVALID TYPE]");
   return 1;
}

//********************************************************************************************************************
// Register the array interface.

void register_array_class(lua_State *Lua)
{
   pf::Log log;

   static const struct luaL_Reg functions[] = {
      { "new",  array_new },
      { NULL, NULL }
   };

   static const struct luaL_Reg methods[] = {
      { "__index",    array_get },
      { "__newindex", array_set },
      { "__len",      array_len },
      { "__gc",       array_destruct },
      { "__tostring", array_tostring },
      { NULL, NULL }
   };

   log.trace("Registering array interface.");

   luaL_newmetatable(Lua, "Fluid.array");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // Push the Fluid.array metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable
   luaL_openlib(Lua, NULL, methods, 0);

   luaL_openlib(Lua, "array", functions, 0);
}
