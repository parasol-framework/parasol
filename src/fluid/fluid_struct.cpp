/*********************************************************************************************************************

To create a struct definition:                    MAKESTRUCT('XMLTag', 'Definition')
To create a struct from a registered definition:  xmltag = struct.new('XMLTag')
To create a struct with pre-configured values:    xmltag = struct.new('XMLTag', { name='Hello' })
To get the byte size of any structure definition: size = struct.size('XMLTag')
To get the total number of fields in a structure: #xmltag
To get the byte size of a created structure:      xmltag.structsize()

Acceptable field definitions:

  l = Long
  d = Double
  x = Large
  f = Float
  w = Word
  b = Byte
  c = Char (If used in an array, array will be interpreted as a string)
  p = Pointer (For a pointer to refer to another structure, use the suffix ':StructName')
  s = String
  m = MaxInt
  o = Object (Pointer)
  r = Function (Embedded)
  e = Embedded structure (e.g. 'eColour:RGB' would embed an RGB structure)

Prefixes for variants, in order of acceptable usage:

  z = Use the C++ variant of the type, e.g. 'cs' for std::string
  u = Unsigned (Use in conjunction with a type)

Arrays are permitted if you follow the field name with [n] where 'n' is the array size.  For pointers to null
terminated arrays, use [0].

*********************************************************************************************************************/

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

static const LONG MAX_STRUCT_DEF = 2048; // Struct definitions are typically 100 - 400 bytes.

//********************************************************************************************************************
// Create a standard Lua table and copy the struct values to that table.  Pushes nil if there was a conversion issue.
// Note the use of the References lookup, which prevents circular referencing and duplication of existing structs.
//
// NOTE: In the event of an error code being returned, no value is pushed to the stack.

ERROR named_struct_to_table(lua_State *Lua, const std::string StructName, CPTR Address)
{
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   auto def = prv->Structs.find(StructName); // NB: Custom comparator will stop if a colon is encountered in StructName
   if (def != prv->Structs.end()) {
      std::vector<lua_ref> ref;
      return struct_to_table(Lua, ref, def->second, Address);
   }
   else if (StructName.starts_with("KeyValue")) {
      // A struct name of 'KeyValue' allows the KEYVALUE type to be used for building structures dynamically.
      // std::map<std::string, std::string>

      return keyvalue_to_table(Lua, (const KEYVALUE *)Address);
   }
   else {
      pf::Log log(__FUNCTION__);
      log.warning("Unknown struct name '%s'", StructName.c_str());
      return ERR_Search;
   }
}

//********************************************************************************************************************

ERROR keyvalue_to_table(lua_State *Lua, const KEYVALUE *Map)
{
   if (!Map) { lua_pushnil(Lua); return ERR_Okay; }

   lua_createtable(Lua, 0, Map->size()); // Create a new table on the stack.

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   for (auto & [ key, val ] : *Map) {
      lua_pushlstring(Lua, key.c_str(), key.size());
      lua_pushlstring(Lua, val.c_str(), val.size());
      lua_settable(Lua, -3);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

ERROR struct_to_table(lua_State *Lua, std::vector<lua_ref> &References, struct_record &StructDef, CPTR Address)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Data: %p", Address);

   // Do not push a Lua value in the event of an error.

   if (!Address) { lua_pushnil(Lua); return ERR_Okay; }

   // Check if there is an existing struct table already associated with this address.  If so, return it
   // rather than creating another table.

   for (auto &rec : References) {
      if (Address IS rec.Address) {
         lua_rawgeti(Lua, LUA_REGISTRYINDEX, rec.Ref);
         return ERR_Okay;
      }
   }

   lua_createtable(Lua, 0, StructDef.Fields.size()); // Create a new table on the stack.

   // Record the address associated with the newly created table.  This is necessary because there may be circular
   // references to it.

   LONG table_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);
   References.push_back({ Address, table_ref });
   lua_rawgeti(Lua, LUA_REGISTRYINDEX, table_ref); // Retrieve the struct table

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   for (auto &field : StructDef.Fields) {

      lua_pushstring(Lua, field.Name.c_str());

      CPTR address = (BYTE *)Address + field.Offset;
      auto type = field.Type;

      if (type & FD_ARRAY) {
         if (type & FD_CPP) {
            auto vector = (pf::vector<int> *)(address);
            if (type & FD_STRUCT) {
               if (prv->Structs.contains(field.StructRef)) {
                  make_any_table(Lua, type, field.StructRef.c_str(), vector->size(), vector->data());
               }
               else lua_pushnil(Lua);
            }
            else make_table(Lua, type, vector->size(), vector->data());
         }
         else if (field.ArraySize IS -1) { // Pointer to a null-terminated array.
            if (type & FD_STRUCT) {
               if (prv->Structs.contains(field.StructRef)) {
                  if (((CPTR *)address)[0]) make_any_table(Lua, type, field.StructRef.c_str(), -1, address);
                  else lua_pushnil(Lua);
               }
               else lua_pushnil(Lua);
            }
            else make_table(Lua, type, -1, ((CPTR *)address)[0]);
         }
         else { // It's an embedded array of fixed size.
            if (type & FD_STRUCT) {
               if (prv->Structs.contains(field.StructRef)) {
                  make_any_table(Lua, type, field.StructRef.c_str(), field.ArraySize, address);
               }
               else lua_pushnil(Lua);
            }
            else make_table(Lua, type, field.ArraySize, address);
         }
      }
      else if (type & FD_STRUCT) {
         auto def = prv->Structs.find(field.StructRef);
         if (def != prv->Structs.end()) {
            if (type & FD_PTR) {
               if (((APTR *)address)[0]) {
                  if (struct_to_table(Lua, References, def->second, ((APTR *)address)[0]) != ERR_Okay) lua_pushnil(Lua);
               }
               else lua_pushnil(Lua);
            }
            else if (struct_to_table(Lua, References, def->second, address) != ERR_Okay) lua_pushnil(Lua);
         }
      }
      else if (type & FD_STRING) {
         if (type & FD_CPP) lua_pushstring(Lua, (((std::string *)address)[0]).c_str());
         else lua_pushstring(Lua, ((STRING *)address)[0]);
      }
      else if (type & FD_OBJECT) push_object(Lua, ((OBJECTPTR *)address)[0]);
      else if (type & FD_POINTER) {
         if (((APTR *)address)[0]) lua_pushlightuserdata(Lua, ((APTR *)address)[0]);
         else lua_pushnil(Lua);
      }
      else if (type & FD_FLOAT)  lua_pushnumber(Lua, ((FLOAT *)address)[0]);
      else if (type & FD_DOUBLE) lua_pushnumber(Lua, ((DOUBLE *)address)[0]);
      else if (type & FD_LARGE)  lua_pushnumber(Lua, ((LARGE *)address)[0]);
      else if (type & FD_LONG)   lua_pushinteger(Lua, ((LONG *)address)[0]);
      else if (type & FD_WORD)   lua_pushinteger(Lua, ((WORD *)address)[0]);
      else if (type & FD_BYTE)   lua_pushinteger(Lua, ((UBYTE *)address)[0]);
      else lua_pushnil(Lua);

      lua_settable(Lua, -3);
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Use this for creating a struct on the Lua stack.

struct fstruct * push_struct(objScript *Self, APTR Address, const std::string &StructName, bool Deallocate, bool AllowEmpty)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Struct: %s, Address: %p, Deallocate: %d", StructName.c_str(), Address, Deallocate);

   auto prv = (prvFluid *)Self->ChildPrivate;
   auto def = prv->Structs.find(StructName);
   if (def != prv->Structs.end()) {
      return push_struct_def(prv->Lua, Address, def->second, Deallocate);
   }
   else if (AllowEmpty) {
      // The AllowEmpty option is useful in situations where a successful API call returns a structure that is strictly
      // unavailable to Fluid.  Rather than throw an exception because the structure isn't in the dictionary, we return
      // an empty structure declaration.

      static struct_record empty("");
      return push_struct_def(prv->Lua, Address, empty, false);
   }
   else {
      if (Deallocate) FreeResource(Address);
      luaL_error(prv->Lua, "Unrecognised struct '%s'", StructName.c_str());
      return NULL;
   }
}

struct fstruct * push_struct_def(lua_State *Lua, APTR Address, struct_record &StructDef, bool Deallocate)
{
   if (auto fs = (fstruct *)lua_newuserdata(Lua, sizeof(fstruct))) {
      fs->Data        = Address;
      fs->Def         = &StructDef;
      fs->StructSize  = StructDef.Size;
      fs->AlignedSize = ALIGN64(StructDef.Size);
      fs->Deallocate  = Deallocate;
      luaL_getmetatable(Lua, "Fluid.struct");
      lua_setmetatable(Lua, -2);
      return fs;
   }
   else luaL_error(Lua, "Failed to create new struct.");

   return NULL;
}

//********************************************************************************************************************
// Lua Usage: structdef = MAKESTRUCT(Name, Sequence)
//
// This function makes a structure definition which can be passed to struct.new()

int MAKESTRUCT(lua_State *Lua)
{
   CSTRING sequence, name;
   if (!(name = lua_tostring(Lua, 1))) luaL_argerror(Lua, 1, "Structure name required.");
   else if (!(sequence = lua_tostring(Lua, 2))) luaL_argerror(Lua, 2, "Structure definition required.");
   else make_struct(Lua, name, sequence);
   return 0;
}

//********************************************************************************************************************
// Camel-case adjustment for field names.  Has to handle cases like IPAddress -> ipAddress; ID -> id

static void make_camel_case(std::string &String)
{
   if ((String[0] >= 'A') and (String[0] <= 'Z')) String[0] = String[0] - 'A' + 'a';

   if ((String[1] >= 'A') and (String[1] <= 'Z')) {
      LONG f;
      for (f=2; String[f]; f++) {
         if ((String[f] >= 'a') and (String[f] <= 'z')) break;
      }

      if (!String[f]) { // Field is all upper-case
         for (LONG f=0; String[f]; f++) {
            if ((String[f] >= 'A') and (String[f] <= 'Z')) String[f] = String[f] - 'A' + 'a';
         }
      }
      else {
         bool lcase = false;
         for (f=1; String[f]; f++) {
            if ((String[f] >= 'A') and (String[f] <= 'Z')) {
               if (lcase) String[f-1] = String[f-1] - 'A' + 'a';
               lcase = true;
            }
            else break;
         }
      }
   }
}

//********************************************************************************************************************
// The TypeName is optional and usually refers to the name of a struct.  The list is sorted by name for fast lookups.

static ERROR generate_structdef(objScript *Self, const std::string StructName, const std::string Sequence,
   struct_record &Record, LONG *StructSize)
{
   pf::Log log(__FUNCTION__);
   auto prv = (prvFluid *)Self->ChildPrivate;
   size_t pos = 0;
   LONG offset = 0;

   while (pos < Sequence.size()) {
      struct_field field;
      LONG type = 0, field_size;

      if (Sequence[pos] IS 'z') {
         type |= FD_CPP;
         pos++;
      }

      if (Sequence[pos] IS 'u') {
         type |= FD_UNSIGNED;
         pos++;
      }

      switch (Sequence[pos]) {
         case 'l': type |= FD_LONG;     field_size = sizeof(LONG); break;
         case 'd': type |= FD_DOUBLE;   field_size = sizeof(DOUBLE); break;
         case 'x': type |= FD_LARGE;    field_size = sizeof(LARGE); break;
         case 'f': type |= FD_FLOAT;    field_size = sizeof(FLOAT); break;
         case 'r': type |= FD_FUNCTION; field_size = sizeof(FUNCTION); break;
         case 'w': type |= FD_WORD;     field_size = sizeof(WORD); break;
         case 'b': type |= FD_BYTE;     field_size = sizeof(UBYTE); break;
         case 'c': type |= FD_BYTE|FD_CUSTOM; field_size = sizeof(UBYTE); break;
         case 'p': type |= FD_POINTER;  field_size = sizeof(APTR); break;

         case 'o': type |= FD_OBJECT;   field_size = sizeof(OBJECTPTR); break;

         case 's':
            type |= FD_STRING;
            if (type & FD_CPP) field_size = sizeof(std::string);
            else field_size = sizeof(STRING);
            break;

         case 'e': { // Embedded structure in the format "eName:Struct[Size]" where [Size] is optional.
            type |= FD_STRUCT;
            auto sep = Sequence.find_first_of(":,[", pos+1);
            if ((sep != std::string::npos) and (Sequence[sep] IS ':')) {
               sep++;
               auto end = Sequence.find_first_of(",[", sep);
               if (end IS std::string::npos) end = Sequence.size();
               auto name = Sequence.substr(sep, end-sep);

               auto def = prv->Structs.find(name);
               if (def != prv->Structs.end()) {
                  field_size = def->second.Size;
                  break;
               }
               else {
                  log.warning("Failed to find referenced struct '%s'", name.c_str());
                  return ERR_NotFound;
               }
            }
            else return ERR_Syntax;
         }

         case 'm': // MAXINT
            type |= (sizeof(MAXINT) IS 4) ? FD_LONG : FD_LARGE;
            field_size = sizeof(MAXINT);
            break;

         default:
            return ERR_Syntax;
      }

      pos++;

      auto i = Sequence.find_first_of(",[:", pos);
      if (i IS std::string::npos) i = Sequence.size();
      field.Name.assign(Sequence, pos, i-pos);
      pos = i;

      // If a struct reference follows the field name, output it and add FD_STRUCT to the type.

      if (Sequence[pos] IS ':') {
         pos++;
         auto i = Sequence.find_first_of(",[", pos);
         if (i IS std::string::npos) i = Sequence.size();
         field.StructRef.assign(Sequence, pos, i-pos);
         type |= FD_STRUCT;
         pos = i;
      }

      make_camel_case(field.Name);

      // Manage fields that are based on fixed array sizes.  NOTE: An array size of zero, i.e. [0] is an indicator
      // that the field is a pointer to a null terminated array.

      LONG array_size = 1;
      if (Sequence[pos] IS '[') {
         pos++;
         type |= FD_ARRAY;
         if (type & FD_CPP) { // In the case of std::vector, fixed array sizes are meaningless
            field_size = sizeof(std::vector<int>);
         }
         else {
            array_size = StrToInt(Sequence.c_str() + pos);
         }
         pos = Sequence.find_first_of("],", pos);
         if (pos IS std::string::npos) pos = Sequence.size();
         else pos++;
      }

      // Alignment and offset management

      if ((field_size >= 8) and (type != FD_STRUCT)) {
         if (offset & 7) log.msg("Warning: %s.%s (%d bytes) is mis-aligned.", StructName.c_str(), field.Name.c_str(), field_size);
         offset = ALIGN64(offset); // 64-bit alignment
      }
      else if (field_size IS 4) offset = ALIGN32(offset);
      else if ((field_size IS 2) and (offset & 1)) offset++; // 16-bit alignment

      pos = Sequence.find(',', pos);

      field.Offset    = offset;
      field.Type      = type;
      field.ArraySize = array_size ? array_size : -1;

      log.trace("Added field %s @ offset %d", field.Name.c_str(), offset);

      if (array_size) offset += field_size * array_size;
      else offset += sizeof(APTR); // Use of [0] indicates a ptr to a null-terminated array.

      Record.Fields.push_back(field);

      while ((pos < Sequence.size()) and ((Sequence[pos] <= 0x20) or (Sequence[pos] IS ','))) pos++;
   }

   *StructSize = offset;
   return ERR_Okay;
}

//********************************************************************************************************************
// Parse a struct definition and permanently store it in the Structs dictionary.

ERROR make_struct(lua_State *Lua, const std::string &StructName, CSTRING Sequence)
{
   if (!Sequence) {
      luaL_error(Lua, "Missing struct name and/or definition.");
      return ERR_NullArgs;
   }

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   if (prv->Structs.contains(StructName)) {
      luaL_error(Lua, "Structure '%s' is already registered.", StructName.c_str());
      return ERR_Exists;
   }

   pf::Log log(__FUNCTION__);
   log.traceBranch("%s, %.50s", StructName.c_str(), Sequence);

   prv->Structs[StructName] = struct_record(StructName);

   LONG computed_size = 0;
   if (auto error = generate_structdef(Lua->Script, StructName, Sequence, prv->Structs[StructName], &computed_size)) {
      if (error IS ERR_BufferOverflow) luaL_argerror(Lua, 1, "String too long - buffer overflow");
      else if (error IS ERR_Syntax) luaL_error(Lua, "Unsupported struct character in definition: %s", Sequence);
      else luaL_error(Lua, "Failed to make struct for %s, error: %s", StructName.c_str(), GetErrorMsg(error));
      return error;
   }

   if (glStructSizes.contains(StructName)) prv->Structs[StructName].Size = glStructSizes[StructName];
   else prv->Structs[StructName].Size = computed_size;

   return ERR_Okay;
}

//********************************************************************************************************************

static struct_field * find_field(struct fstruct *Struct, CSTRING FieldName)
{
   if (auto def = Struct->Def) {
      auto field_hash = StrHash(FieldName);
      for (auto &field : def->Fields) {
         if (field.nameHash() IS field_hash) return &field;
      }
   }

   return NULL;
}

//********************************************************************************************************************
// Usage: struct = struct.size(Name)
//
// Returns the size of a named structure definition

static int struct_size(lua_State *Lua)
{
   if (auto name = lua_tostring(Lua, 1)) {
      auto prv = (prvFluid *)Lua->Script->ChildPrivate;
      auto def = prv->Structs.find(struct_name(name));
      if (def != prv->Structs.end()) {
         lua_pushnumber(Lua, def->second.Size);
         return 1;
      }
      else luaL_argerror(Lua, 1, "The requested structure is not defined.");
   }
   else luaL_argerror(Lua, 1, "Structure name required.");
   return 0;
}

/*********************************************************************************************************************
** Usage: struct = struct.new(Name)
**
** Creates a new structure.  The name of the structure must have been previously registered, either through an include
** file or by calling MAKESTRUCT.
*/

static int struct_new(lua_State *Lua)
{
   if (auto s_name = lua_tostring(Lua, 1)) {
      auto prv = (prvFluid *)Lua->Script->ChildPrivate;

      auto def = prv->Structs.find(struct_name(s_name));
      if (def IS prv->Structs.end()) {
         luaL_argerror(Lua, 1, "The requested structure is not defined.");
         return 0;
      }

      auto &record = def->second;
      if (auto fs = (fstruct *)lua_newuserdata(Lua, sizeof(fstruct) + record.Size)) {
         luaL_getmetatable(Lua, "Fluid.struct");
         lua_setmetatable(Lua, -2);

         fs->Data  = (APTR)(fs + 1);
         ClearMemory(fs->Data, record.Size);

         fs->Def         = &record;
         fs->StructSize  = record.Size;
         fs->AlignedSize = ALIGN64(record.Size);
         fs->Deallocate  = false;

         if (lua_istable(Lua, 2)) {
            pf::Log log(__FUNCTION__);
            log.trace("struct.new(%p, fields: %d)", record, record.Fields.size());
            ERROR field_error = ERR_Okay;
            lua_pushnil(Lua);  // Access first key for lua_next()
            while (lua_next(Lua, 2) != 0) {
               if (auto field_name = luaL_checkstring(Lua, -2)) {
                  if (auto field = find_field(fs, field_name)) {
                     log.trace("struct.set() Offset %d, $%.8x", field->Offset, field->Type);
                     APTR address = (BYTE *)fs->Data + field->Offset;
                     if (field->Type & FD_STRING) {
                        log.trace("Strings not supported yet.");
                        // In order to set strings, we'd need make a copy of the string received from
                        // Lua and free it when the field changes or the structure is destroyed.
                     }
                     else if (field->Type & FD_OBJECT)  ((OBJECTPTR *)address)[0] = (OBJECTPTR)lua_touserdata(Lua, 3);
                     else if (field->Type & FD_LONG)   ((LONG *)address)[0]   = lua_tointeger(Lua, 3);
                     else if (field->Type & FD_WORD)   ((WORD *)address)[0]   = lua_tointeger(Lua, 3);
                     else if (field->Type & FD_BYTE)   ((BYTE *)address)[0]   = lua_tointeger(Lua, 3);
                     else if (field->Type & FD_DOUBLE) ((DOUBLE *)address)[0] = lua_tonumber(Lua, 3);
                     else if (field->Type & FD_FLOAT)  ((FLOAT *)address)[0]  = lua_tonumber(Lua, 3);
                     else log.warning("Cannot set unsupported field type for %s", field_name);
                  }
                  else field_error = ERR_UnsupportedField;
               }
               else field_error = ERR_UnsupportedField;

               if (field_error) { // Break the loop early on error.
                  lua_pop(Lua, 2);
                  break;
               }
               else lua_pop(Lua, 1);  // removes 'value'; keeps 'key' for the proceeding lua_next() iteration
            }
         }

         return 1;  // new userdatum is already on the stack
      }
      else luaL_error(Lua, "Failed to create new struct.");
   }
   else luaL_argerror(Lua, 1, "Structure name required.");

   return 0;
}

//********************************************************************************************************************
// Usage: struct.size()
// Returns the byte size of the structure definition.

static int struct_structsize(lua_State *Lua)
{
   if (auto fs = (fstruct *)get_meta(Lua, lua_upvalueindex(1), "Fluid.struct")) {
      lua_pushnumber(Lua, fs->StructSize);
      return 1;
   }
   else {
      luaL_argerror(Lua, 1, "Expected struct.");
      return 0;
   }
}

//********************************************************************************************************************
// Usage: #struct
// Returns the total number of fields in the structure definition.

static int struct_len(lua_State *Lua)
{
   if (auto fs = (struct fstruct *)lua_touserdata(Lua, 1)) {
      lua_pushnumber(Lua, fs->Def->Fields.size());
      return 1;
   }
   else {
      luaL_argerror(Lua, 1, "Expected struct.");
      return 0;
   }
}

//********************************************************************************************************************
// Struct index call

static int struct_get(lua_State *Lua)
{
   if (auto fs = (struct fstruct *)lua_touserdata(Lua, 1)) {
      if (auto fieldname = luaL_checkstring(Lua, 2)) {
         if (!StrCompare("structsize", fieldname, 0, STR::MATCH_CASE)) {
            lua_pushvalue(Lua, 1);
            lua_pushcclosure(Lua, &struct_structsize, 1);
            return 1;
         }

         if (!fs->Data) {
            luaL_error(Lua, "Cannot reference field '%s' because struct address is NULL.", fieldname);
            return 0;
         }

         if (auto field = find_field(fs, fieldname)) {
            APTR address = (BYTE *)fs->Data + field->Offset;
            LONG array_size = (!field->ArraySize) ? -1 : field->ArraySize;

            if ((field->Type & FD_STRUCT) and (field->Type & FD_PTR) and (!field->StructRef.empty())) { // Pointer to structure
               if (((APTR *)address)[0]) {
                  if (field->Type & FD_ARRAY) { // Array of pointers to structures.
                     if (field->Type & FD_CPP) {
                        auto vector = (pf::vector<int> *)(address);
                        make_array(Lua, field->Type, field->StructRef.c_str(), (APTR *)vector->data(), vector->size(), false);
                     }
                     else make_array(Lua, field->Type, field->StructRef.c_str(), (APTR *)address, array_size, false);
                  }
                  else push_struct(Lua->Script, ((APTR *)address)[0], field->StructRef, false, false);
               }
               else lua_pushnil(Lua);
            }
            else if (field->Type & FD_STRUCT) { // Embedded structure
               push_struct(Lua->Script, address, field->StructRef, false, false);
            }
            else if (field->Type & FD_STRING) {
               if (field->Type & FD_ARRAY) {
                  if (field->Type & FD_CPP) {
                     auto vector = (pf::vector<std::string> *)(address);
                     make_array(Lua, FD_CPP|FD_STRING, NULL, (APTR *)vector->data(), vector->size(), false);
                  }
                  else make_array(Lua, FD_STRING, NULL, (APTR *)address, array_size, false);
               }
               else if (field->Type & FD_CPP) {
                  lua_pushstring(Lua, ((std::string *)address)->c_str());
               }
               else lua_pushstring(Lua, ((STRING *)address)[0]);
            }
            else if (field->Type & FD_OBJECT) {
               push_object(Lua, ((OBJECTPTR *)address)[0]);
            }
            else if (field->Type & FD_POINTER) {
               if (((APTR *)address)[0]) lua_pushlightuserdata(Lua, ((APTR *)address)[0]);
               else lua_pushnil(Lua);
            }
            else if (field->Type & FD_FUNCTION) {
               lua_pushnil(Lua);
            }
            else if (field->Type & FD_FLOAT)   {
               if (field->Type & FD_ARRAY) make_array(Lua, FD_FLOAT, NULL, (APTR *)address, array_size, false);
               else lua_pushnumber(Lua, ((FLOAT *)address)[0]);
            }
            else if (field->Type & FD_DOUBLE) {
               if (field->Type & FD_ARRAY) make_array(Lua, FD_DOUBLE, NULL, (APTR *)address, array_size, false);
               else lua_pushnumber(Lua, ((DOUBLE *)address)[0]);
            }
            else if (field->Type & FD_LARGE) {
               if (field->Type & FD_ARRAY) make_array(Lua, FD_LARGE, NULL, (APTR *)address, array_size, false);
               else lua_pushnumber(Lua, ((LARGE *)address)[0]);
            }
            else if (field->Type & FD_LONG) {
               if (field->Type & FD_ARRAY) make_array(Lua, FD_LONG, NULL, (APTR *)address, array_size, false);
               else lua_pushinteger(Lua, ((LONG *)address)[0]);
            }
            else if (field->Type & FD_WORD) {
               if (field->Type & FD_ARRAY) make_array(Lua, FD_WORD, NULL, (APTR *)address, array_size, false);
               else lua_pushinteger(Lua, ((WORD *)address)[0]);
            }
            else if (field->Type & FD_BYTE) {
               if ((field->Type & FD_CUSTOM) and (field->Type & FD_ARRAY)) {
                  // Character arrays are interpreted as strings.  Use 'b' instead of 'c' if this behaviour is undesirable
                  lua_pushstring(Lua, (CSTRING)address);
               }
               else if (field->Type & FD_ARRAY) make_array(Lua, FD_BYTE, NULL, (APTR *)address, array_size, false);
               else lua_pushinteger(Lua, ((UBYTE *)address)[0]);
            }
            else {
               char buffer[80];
               snprintf(buffer, sizeof(buffer), "Field '%s' does not use a supported type ($%.8x).", fieldname, field->Type);
               luaL_error(Lua, buffer);
               return 0;
            }
            return 1;
         }
         else {
            luaL_error(Lua, "Field '%s' does not exist in structure.", fieldname);
            return 0;
         }
      }
   }

   return 0;
}

//********************************************************************************************************************
// Usage: fstruct.field = newvalue

static int struct_set(lua_State *Lua)
{
   if (auto fs = (struct fstruct *)lua_touserdata(Lua, 1)) {
      if (auto ref = luaL_checkstring(Lua, 2)) {
         if (!fs->Data) {
            luaL_error(Lua, "Cannot reference field '%s' because struct address is NULL.", ref);
            return 0;
         }

         if (auto field = find_field(fs, ref)) {
            pf::Log log;
            log.trace("struct.set() %s, Offset %d, $%.8x", ref, field->Offset, field->Type);

            APTR address = (BYTE *)fs->Data + field->Offset;

            if (field->Type & FD_STRING) {
               log.trace("Strings not supported yet.");
               // In order to set strings, we'd need make a copy of the string received from
               // Lua and free it when the field changes or the structure is destroyed.

            }
            else if (field->Type & FD_OBJECT)  ((OBJECTPTR *)address)[0] = (OBJECTPTR)lua_touserdata(Lua, 3);
            else if (field->Type & FD_POINTER) ((APTR *)address)[0] = lua_touserdata(Lua, 3);
            else if (field->Type & FD_FUNCTION);
            else if (field->Type & FD_LONG)   ((LONG *)address)[0]   = lua_tointeger(Lua, 3);
            else if (field->Type & FD_WORD)   ((WORD *)address)[0]   = lua_tointeger(Lua, 3);
            else if (field->Type & FD_BYTE)   ((BYTE *)address)[0]   = lua_tointeger(Lua, 3);
            else if (field->Type & FD_DOUBLE) ((DOUBLE *)address)[0] = lua_tonumber(Lua, 3);
            else if (field->Type & FD_FLOAT)  ((FLOAT *)address)[0]  = lua_tonumber(Lua, 3);
         }
         else luaL_error(Lua, "Invalid field reference '%s'", ref);
      }
      else luaL_error(Lua, "Translation failure.");
   }

   return 0;
}

//********************************************************************************************************************
// Garbage collecter.

static int struct_destruct(lua_State *Lua)
{
   if (auto fs = (fstruct *)luaL_checkudata(Lua, 1, "Fluid.struct")) {
      if (fs->Deallocate) {
         FreeResource(fs->Data);
         fs->Data = NULL;
      }
   }

   return 0;
}

//********************************************************************************************************************
// Register the fstruct interface.

static const luaL_Reg structlib_functions[] = {
   { "new",   struct_new },
   { "size",  struct_size },
   { NULL, NULL }
};

static const luaL_Reg structlib_methods[] = {
   { "__index",    struct_get },
   { "__newindex", struct_set },
   { "__len",      struct_len },
   { "__gc",       struct_destruct },
   { NULL, NULL }
};

void register_struct_class(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);
   log.trace("Registering struct interface.");

   luaL_newmetatable(Lua, "Fluid.struct");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable
   luaL_openlib(Lua, NULL, structlib_methods, 0);

   luaL_openlib(Lua, "struct", structlib_functions, 0);
}
