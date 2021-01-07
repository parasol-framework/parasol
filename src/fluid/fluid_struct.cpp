/*****************************************************************************

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
  u = Unsigned (Use in conjunction with a type)
  e = Embedded structure (e.g. 'eColour:RGB' would embed an RGB structure)

Arrays are permitted if you follow the field name with [n] where 'n' is the array size.  For pointers to null
terminated arrays, use [0].

*****************************************************************************/

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

#define MAX_STRUCT_DEF 2048 // Struct definitions are typically 100 - 400 bytes.

static LONG get_fieldvalue(lua_State *, struct fstruct *, CSTRING);

static LONG get_ptr_ref(struct references *Ref, CPTR Address)
{
   for (LONG i=0; i < Ref->Index; i++) {
      if (Address IS Ref->List[i].Address) return Ref->List[i].Ref;
   }
   return 0;
}

static void set_ptr_ref(struct references *Ref, CPTR Address, LONG Resource)
{
   if (Ref->Index < ARRAYSIZE(Ref->List)-1) {
      LONG i = Ref->Index++;
      Ref->List[i].Address = Address;
      Ref->List[i].Ref = Resource;
   }
}

//****************************************************************************
// Create a standard Lua table and copy the struct values to that table.  Pushes nil if there was a conversion issue.
// Note the use of the References lookup, which prevents circular referencing and duplication of existing structs.
//
// NOTE: In the event of an error code being returned, no value is pushed to the stack.

ERROR named_struct_to_table(lua_State *Lua, CSTRING StructName, APTR Address)
{
   auto prv = (prvFluid *)Lua->Script->Head.ChildPrivate;
   structentry *def;
   if (!KeyGet(prv->Structs, STRUCTHASH(StructName), &def, NULL)) {
      return struct_to_table(Lua, NULL, def, Address);
   }
   else {
      parasol::Log log(__FUNCTION__);
      log.warning("Unknown struct name '%s'", StructName);
      return ERR_Search;
   }
}

ERROR struct_to_table(lua_State *Lua, struct references *References, struct structentry *StructDef, CPTR Address)
{
   parasol::Log log(__FUNCTION__);

   // Do not push a Lua value in the event of an error.

   log.traceBranch("Data: %p, StructDef: %p, References: %p, Index: %d", Address, StructDef, References, References ? References->Index : -1);

   if (!Address) { lua_pushnil(Lua); return ERR_Okay; }

   BYTE free_ref = FALSE;
   if (!References) {
      References = alloc_references();
      if (!References) { return ERR_AllocMemory; }
      free_ref = TRUE;
   }

   // Check if there is an existing struct table already associated with this address.  If so, return it
   // rather than creating another table.

   LONG existing_ref = get_ptr_ref(References, Address);
   if (existing_ref) {
      lua_rawgeti(Lua, LUA_REGISTRYINDEX, existing_ref);
      return ERR_Okay;
   }

   lua_createtable(Lua, 0, StructDef->Total); // Create a new table on the stack.

   // Record the address associated with the newly created table.  This is necessary because there may be circular
   // references to it.

   LONG table_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);
   set_ptr_ref(References, Address, table_ref);
   lua_rawgeti(Lua, LUA_REGISTRYINDEX, table_ref); // Retrieve the struct table

   auto prv = (prvFluid *)Lua->Script->Head.ChildPrivate;

   auto field = (structdef_field *)(StructDef + 1);
   for (LONG f=0; f < StructDef->Total; f++, field=(structdef_field *)((BYTE *)field + field->Length)) {
      CSTRING field_name = (CSTRING)(field + 1);
      lua_pushstring(Lua, field_name);

      CPTR address = (BYTE *)Address + field->Offset;
      LONG type = field->Type;

      if (type & FD_ARRAY) {
         if (field->ArraySize IS -1) { // Pointer to a null-terminated array.
            if (type & FD_STRUCT) {
               CSTRING struct_name = ((CSTRING)(field + 1)) + field->StructOffset;
               structentry *def;
               if (!VarGet(prv->Structs, struct_name, &def, NULL)) {
                  if (((CPTR *)address)[0]) make_any_table(Lua, type, struct_name, -1, address);
                  else lua_pushnil(Lua);
               }
            }
            else if (type & FD_STRING)  make_table(Lua, FD_STRING, -1, address);
            else if (type & FD_OBJECT)  make_table(Lua, FD_OBJECT, -1, ((CPTR *)address)[0]);
            else if (type & FD_POINTER) make_table(Lua, FD_POINTER, -1, ((CPTR *)address)[0]);
            else if (type & FD_FLOAT)   make_table(Lua, FD_FLOAT, -1, ((CPTR *)address)[0]);
            else if (type & FD_DOUBLE)  make_table(Lua, FD_DOUBLE, -1, ((CPTR *)address)[0]);
            else if (type & FD_LARGE)   make_table(Lua, FD_LARGE, -1, ((CPTR *)address)[0]);
            else if (type & FD_LONG)    make_table(Lua, FD_LONG, -1, ((CPTR *)address)[0]);
            else if (type & FD_WORD)    make_table(Lua, FD_WORD, -1, ((CPTR *)address)[0]);
            else if (type & FD_BYTE)    make_table(Lua, FD_BYTE, -1, ((CPTR *)address)[0]);
            else lua_pushnil(Lua);
         }
         else { // It's an embedded array of fixed size.
            if (type & FD_STRUCT) {
               CSTRING struct_name = ((CSTRING)(field + 1)) + field->StructOffset;
               structentry *def;
               if (!VarGet(prv->Structs, struct_name, &def, NULL)) {
                  make_any_table(Lua, type, struct_name, field->ArraySize, address);
               }
            }
            else if (type & FD_STRING)  make_table(Lua, FD_STRING, field->ArraySize, address);
            else if (type & FD_OBJECT)  make_table(Lua, FD_OBJECT, field->ArraySize, address);
            else if (type & FD_POINTER) make_table(Lua, FD_POINTER, field->ArraySize, address);
            else if (type & FD_FLOAT)   make_table(Lua, FD_FLOAT, field->ArraySize, address);
            else if (type & FD_DOUBLE)  make_table(Lua, FD_DOUBLE, field->ArraySize, address);
            else if (type & FD_LARGE)   make_table(Lua, FD_LARGE, field->ArraySize, address);
            else if (type & FD_LONG)    make_table(Lua, FD_LONG, field->ArraySize, address);
            else if (type & FD_WORD)    make_table(Lua, FD_WORD, field->ArraySize, address);
            else if (type & FD_BYTE)    make_table(Lua, FD_BYTE, field->ArraySize, address);
            else lua_pushnil(Lua);
         }
      }
      else if (type & FD_STRUCT) {
         CSTRING struct_name = ((CSTRING)(field + 1)) + field->StructOffset;
         structentry *def;
         if (!VarGet(prv->Structs, struct_name, &def, NULL)) {
            if (type & FD_PTR) {
               if (((APTR *)address)[0]) {
                  if (struct_to_table(Lua, References, def, ((APTR *)address)[0]) != ERR_Okay) lua_pushnil(Lua);
               }
               else lua_pushnil(Lua);
            }
            else if (struct_to_table(Lua, References, def, address) != ERR_Okay) lua_pushnil(Lua);
         }
      }
      else if (type & FD_STRING) lua_pushstring(Lua, ((STRING *)address)[0]);
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

   if (free_ref) free_references(Lua, References);

   return ERR_Okay;
}

//****************************************************************************
// Use this for creating a struct on the Lua stack.

struct fstruct * push_struct(objScript *Self, APTR Address, CSTRING StructName, BYTE Deallocate, BYTE AllowEmpty)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("Struct: %s, Address: %p, Deallocate: %d", StructName, Address, Deallocate);

   auto prv = (prvFluid *)Self->Head.ChildPrivate;
   structentry *def;
   if (!KeyGet(prv->Structs, STRUCTHASH(StructName), &def, NULL)) {
      return push_struct_def(prv->Lua, Address, def, Deallocate);
   }
   else if (AllowEmpty) {
      // The AllowEmpty option is useful in situations where a successful API call returns a structure that is strictly
      // unavailable to Fluid.  Rather than throw an exception because the structure isn't in the dictionary, we return
      // an empty structure declaration.

      static structentry empty = { .Total = 0, .Size = 0, .NameHash = 0 };
      return push_struct_def(prv->Lua, Address, &empty, FALSE);
   }
   else {
      if (Deallocate) FreeResource(Address);
      luaL_error(prv->Lua, "Unrecognised struct '%s'", StructName);
      return NULL;
   }
}

struct fstruct * push_struct_def(lua_State *Lua, APTR Address, structentry *StructDef, BYTE Deallocate)
{
   fstruct *fs;
   if ((fs = (fstruct *)lua_newuserdata(Lua, sizeof(fstruct)))) {
      fs->Data        = Address;
      fs->Def         = StructDef;
      fs->StructSize  = StructDef->Size;
      fs->AlignedSize = ALIGN64(StructDef->Size);
      fs->Deallocate  = Deallocate;
      luaL_getmetatable(Lua, "Fluid.struct");
      lua_setmetatable(Lua, -2);
      return fs;
   }
   else luaL_error(Lua, "Failed to create new struct.");

   return NULL;
}

/*****************************************************************************
** Lua Usage: structdef = MAKESTRUCT(Name, Sequence)
**
** This function makes a structure definition which can be passed to struct.new()
*/

int MAKESTRUCT(lua_State *Lua)
{
   CSTRING sequence, name;
   if (!(name = lua_tostring(Lua, 1))) luaL_argerror(Lua, 1, "Structure name required.");
   else if (!(sequence = lua_tostring(Lua, 2))) luaL_argerror(Lua, 2, "Structure definition required.");
   else make_struct(Lua, name, sequence);
   return 0;
}

//****************************************************************************

static ERROR eval_type(objScript *Self, CSTRING Sequence, LONG *Pos, LONG *Type, LONG *Size)
{
   LONG type = 0;
   LONG pos = *Pos;

   if (Sequence[pos] IS 'u') {
      type |= FD_UNSIGNED;
      pos++;
   }

   if (Sequence[pos] IS 'l')      { *Type = type|FD_LONG;     *Size = sizeof(LONG); }
   else if (Sequence[pos] IS 'd') { *Type = type|FD_DOUBLE;   *Size = sizeof(DOUBLE); }
   else if (Sequence[pos] IS 'x') { *Type = type|FD_LARGE;    *Size = sizeof(LARGE); }
   else if (Sequence[pos] IS 'f') { *Type = type|FD_FLOAT;    *Size = sizeof(FLOAT); }
   else if (Sequence[pos] IS 'r') { *Type = type|FD_FUNCTION; *Size = sizeof(FUNCTION); }
   else if (Sequence[pos] IS 'w') { *Type = type|FD_WORD;     *Size = sizeof(WORD); }
   else if (Sequence[pos] IS 'b') { *Type = type|FD_BYTE;     *Size = sizeof(UBYTE); }
   else if (Sequence[pos] IS 'c') { *Type = type|FD_BYTE|FD_CUSTOM; *Size = sizeof(UBYTE); }
   else if (Sequence[pos] IS 'p') { *Type = type|FD_POINTER;  *Size = sizeof(APTR); }
   else if (Sequence[pos] IS 's') { *Type = type|FD_STRING;   *Size = sizeof(STRING); }
   else if (Sequence[pos] IS 'o') { *Type = type|FD_OBJECT;   *Size = sizeof(OBJECTPTR); }
   else if (Sequence[pos] IS 'e') {
      // Embedded structure.  Get the name so that we can determine the struct size.
      *Type = FD_STRUCT;
      LONG i;
      for (i=pos; (Sequence[i]) and (Sequence[i] != ':') and (Sequence[i] != ','); i++);

      if (Sequence[i] IS ':') {
         i++;
         struct structentry *def;
         auto prv = (prvFluid *)Self->Head.ChildPrivate;
         if (!KeyGet(prv->Structs, STRUCTHASH(Sequence+i), &def, NULL)) {
            *Size = def->Size;
         }
         else {
            parasol::Log log(__FUNCTION__);
            log.warning("Failed to find referenced struct '%s'", Sequence+i);
            return ERR_NotFound;
         }
      }
      else return ERR_Syntax;
   }
   else if (Sequence[pos] IS 'm') { // MAXINT
      if (sizeof(MAXINT) IS 4) *Type = type|FD_LONG;
      else *Type = type|FD_LARGE;
      *Size = sizeof(MAXINT);
   }
   else return ERR_Syntax;

   *Pos = pos + 1;
   return ERR_Okay;
}

//****************************************************************************
// The structure definition is arranged as:
//
//    Len:Offset:FieldType:FieldName:TypeName
//    ...
//
// The TypeName is optional and usually refers to the name of a struct.  The list is sorted by name for fast lookups.

static ERROR generate_structdef(objScript *Self, CSTRING StructName, CSTRING Sequence, BYTE *Buffer, LONG *Offset,
      LONG *TotalFields, LONG *BufferEnd)
{
   parasol::Log log(__FUNCTION__);
   LONG offset = 0;
   LONG pos    = 0;
   LONG total  = 0;
   LONG buf    = 0;
   while (Sequence[pos]) {
      if (buf >= MAX_STRUCT_DEF-128) return ERR_BufferOverflow;

      LONG type, field_size;
      ERROR error;

      if ((error = eval_type(Self, Sequence, &pos, &type, &field_size)) != ERR_Okay) return error;

      // Output the name of the field (following the structdef_field).

      STRING field_name = Buffer + buf + sizeof(struct structdef_field);
      LONG i;
      for (i=0; (Sequence[pos]) and (Sequence[pos] != ',') and (Sequence[pos] != '[') AND
                (Sequence[pos] != ':') and (i < 64); i++) {
         field_name[i] = Sequence[pos++];
      }
      field_name[i++] = 0;

      // If a struct reference follows the field name, output it and add FD_STRUCT to the type.

      LONG struct_offset;
      if (Sequence[pos] IS ':') {
         pos++;
         struct_offset = i; // Index to the struct's name
         while ((Sequence[pos]) and (Sequence[pos] != ',') and (Sequence[pos] != '[') and (i < 64)) {
            field_name[i++] = Sequence[pos++];
         }
         field_name[i++] = 0;
         type |= FD_STRUCT;
      }
      else struct_offset = 0;

      // Camel-case adjustment.  Has to handle cases like IPAddress -> ipAddress

      if ((field_name[0] >= 'A') and (field_name[0] <= 'Z')) field_name[0] = field_name[0] - 'A' + 'a';

      UBYTE lcase = FALSE;
      for (LONG f=1; field_name[f]; f++) {
         if ((field_name[f] >= 'A') and (field_name[f] <= 'Z')) {
            if (lcase) field_name[f-1] = field_name[f-1] - 'A' + 'a';
            lcase = TRUE;
         }
         else break;
      }

      // Alignment and offset management

      LONG entry_size = sizeof(struct structdef_field) + AlignLong(i); // 32-bit alignment applies to each array entry

      if ((field_size >= 8) and (type != FD_STRUCT)) {
         if (offset & 7) log.msg("Warning: %s.%s (%d bytes) is mis-aligned.", StructName, field_name, field_size);
         offset = ALIGN64(offset); // 64-bit alignment
      }
      else if (field_size IS 4) offset = AlignLong(offset);
      else if ((field_size IS 2) and (offset & 1)) offset++; // 16-bit alignment

      // Manage fields that are based on fixed array sizes.  NOTE: An array size of zero, i.e. [0] is an indicator
      // that the field is a pointer to a null terminated array.

      LONG array_size = 1;
      if (Sequence[pos] IS '[') {
         pos++;
         array_size = StrToInt(Sequence+pos);
         type |= FD_ARRAY;
         while ((Sequence[pos]) and (Sequence[pos] != ']') and (Sequence[pos] != ',')) pos++;
         if (Sequence[pos] IS ']') pos++;
      }

      while ((Sequence[pos]) and (Sequence[pos] != ',')) pos++;

      ((structdef_field *)(Buffer+buf))->Length       = entry_size;
      ((structdef_field *)(Buffer+buf))->Offset       = offset;
      ((structdef_field *)(Buffer+buf))->Type         = type;
      ((structdef_field *)(Buffer+buf))->ArraySize    = array_size ? array_size : -1;
      ((structdef_field *)(Buffer+buf))->StructOffset = struct_offset;
      ((structdef_field *)(Buffer+buf))->NameHash     = StrHash(field_name, FALSE);

      log.trace("Added field %s @ offset %d", field_name, offset);

      if (array_size) offset += field_size * array_size;
      else offset += sizeof(APTR); // Pointer to a null-terminated array.
      buf += entry_size;
      total++;

      while ((Sequence[pos]) and ((Sequence[pos] <= 0x20) or (Sequence[pos] IS ','))) pos++;
   }

   *Offset = offset;
   *TotalFields = total;
   *BufferEnd = buf;
   return ERR_Okay;
}

//****************************************************************************
// Parse a struct definition and permanently store it in the Structs keystore.

ERROR make_struct(lua_State *Lua, CSTRING StructName, CSTRING Sequence)
{
   if ((!StructName) or (!Sequence)) {
      luaL_error(Lua, "Missing struct name and/or definition.");
      return ERR_NullArgs;
   }

   auto prv = (prvFluid *)Lua->Script->Head.ChildPrivate;
   if ((prv->Structs) and (!VarGet(prv->Structs, StructName, NULL, NULL))) {
      luaL_error(Lua, "Structure name '%s' is already registered.", StructName);
      return ERR_Exists;
   }

   if ((!prv->Structs) and (!(prv->Structs = VarNew(0, KSF_CASE)))) {
      luaL_error(Lua, "Failed to allocate key-store.");
      return ERR_AllocMemory;
   }

   parasol::Log log(__FUNCTION__);
   log.msg(VLF_BRANCH|VLF_DEBUG, "%s, %.50s", StructName, Sequence);

   UBYTE buffer[sizeof(struct structentry) + MAX_STRUCT_DEF + 8];

   LONG offset = 0, total_fields = 0, buf_end = 0;
   ERROR error;
   if ((error = generate_structdef(Lua->Script, StructName, Sequence, (BYTE *)buffer + sizeof(struct structentry), &offset,
         &total_fields, &buf_end)) != ERR_Okay) {
      log.debranch();
      if (error IS ERR_BufferOverflow) luaL_argerror(Lua, 1, "String too long - buffer overflow");
      else if (error IS ERR_Syntax) luaL_error(Lua, "Unsupported struct character in definition: %s", Sequence);
      else luaL_error(Lua, "Failed to make struct for %s, error: %s", StructName, GetErrorMsg(error));
      return error;
   }

   // Note the 64-bit padding safety net.  GCC can make a struct 64-bit aligned sometimes, e.g. if it contains at least
   // one field that is 64-bit integer or float.  The compiler options -mno-align-double and -malign-double may affect
   // this also.

   buf_end = ALIGN64(buf_end);

   struct structentry *entry = (struct structentry *)buffer;
   entry->Total = total_fields;
   entry->Size  = offset;
   entry->NameHash = STRUCTHASH(StructName);

   log.trace("Struct %s has %d fields, size %d, ref %p", StructName, total_fields, offset, entry);

   if (!VarSet(prv->Structs, StructName, entry, sizeof(struct structentry) + buf_end)) {
      log.debranch();
      luaL_error(Lua, GetErrorMsg(ERR_AllocMemory));
   }

   return error;
}

//****************************************************************************

static LONG find_field(struct fstruct *Struct, CSTRING FieldName, CSTRING *StructName, LONG *Offset, LONG *Type, LONG *ArraySize)
{
   structentry *def;
   if ((def = Struct->Def)) {
      auto fields = (structdef_field *)(def + 1);
      ULONG field_hash = StrHash(FieldName, FALSE);
      for (LONG i=0; i < def->Total; i++) {
         if (fields->NameHash IS field_hash) {
            if ((fields->Type & FD_STRUCT) and (StructName)) {
               *StructName = (CSTRING)(fields + 1) + fields->StructOffset;
            }
            *Offset = fields->Offset;
            *Type = fields->Type;
            if (ArraySize) {
               if (!fields->ArraySize) *ArraySize = -1;
               else *ArraySize = fields->ArraySize;
            }
            return TRUE;
         }
         fields = (structdef_field *)((BYTE *)fields + fields->Length);
      }
   }

   return FALSE;
}

/*****************************************************************************
** Usage: struct = struct.size(Name)
**
** Returns the size of a named structure definition
*/

static int struct_size(lua_State *Lua)
{
   CSTRING name;
   if ((name = lua_tostring(Lua, 1))) {
      auto prv = (prvFluid *)Lua->Script->Head.ChildPrivate;
      structentry *def;
      if (!VarGet(prv->Structs, name, &def, NULL)) {
         lua_pushnumber(Lua, def->Size);
         return 1;
      }
      else luaL_argerror(Lua, 1, "The requested structure is not defined.");
   }
   else luaL_argerror(Lua, 1, "Structure name required.");
   return 0;
}

/*****************************************************************************
** Usage: struct = struct.new(Name)
**
** Creates a new structure.  The name of the structure must have been previously registered, either through an include
** file or by calling MAKESTRUCT.
*/

static int struct_new(lua_State *Lua)
{
   CSTRING struct_name;

   if ((struct_name = lua_tostring(Lua, 1))) {
      auto prv = (prvFluid *)Lua->Script->Head.ChildPrivate;
      structentry *def;
      if (VarGet(prv->Structs, struct_name, &def, NULL)) {
         luaL_argerror(Lua, 1, "The requested structure is not defined.");
         return 0;
      }

      fstruct *fs;
      if ((fs = (fstruct *)lua_newuserdata(Lua, sizeof(fstruct) + def->Size))) {
         luaL_getmetatable(Lua, "Fluid.struct");
         lua_setmetatable(Lua, -2);

         fs->Data  = (APTR)(fs + 1);
         ClearMemory(fs->Data, def->Size);

         fs->Def         = def;
         fs->StructSize  = def->Size;
         fs->AlignedSize = ALIGN64(def->Size);
         fs->Deallocate  = FALSE;

         if (lua_istable(Lua, 2)) {
            parasol::Log log(__FUNCTION__);
            log.trace("struct.new(%p, fields: %d, size: %d)", def, def->Total, def->Size);
            ERROR field_error = ERR_Okay;
            CSTRING field_name = NULL;
            lua_pushnil(Lua);  // Access first key for lua_next()
            while (lua_next(Lua, 2) != 0) {
               if ((field_name = luaL_checkstring(Lua, -2))) {
                  LONG offset, type;
                  if ((find_field(fs, field_name, NULL, &offset, &type, NULL))) {
                     log.trace("struct.set() Offset %d, $%.8x", offset, type);
                     APTR address = (BYTE *)fs->Data + offset;
                     if (type & FD_STRING) {
                        log.trace("Strings not supported yet.");
                        // In order to set strings, we'd need make a copy of the string received from
                        // Lua and free it when the field changes or the structure is destroyed.
                     }
                     else if (type & FD_OBJECT)  ((OBJECTPTR *)address)[0] = (OBJECTPTR)lua_touserdata(Lua, 3);
                     else if (type & FD_LONG)   ((LONG *)address)[0]   = lua_tointeger(Lua, 3);
                     else if (type & FD_WORD)   ((WORD *)address)[0]   = lua_tointeger(Lua, 3);
                     else if (type & FD_BYTE)   ((BYTE *)address)[0]   = lua_tointeger(Lua, 3);
                     else if (type & FD_DOUBLE) ((DOUBLE *)address)[0] = lua_tonumber(Lua, 3);
                     else if (type & FD_FLOAT)  ((FLOAT *)address)[0]  = lua_tonumber(Lua, 3);
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

//****************************************************************************
// Usage: struct.size()
// Returns the byte size of the structure definition.

static int struct_structsize(lua_State *Lua)
{
   auto fs = (fstruct *)get_meta(Lua, lua_upvalueindex(1), "Fluid.struct");
   if (!fs) {
      luaL_argerror(Lua, 1, "Expected struct.");
      return 0;
   }
   else {
      lua_pushnumber(Lua, fs->StructSize);
      return 1;
   }
}

//****************************************************************************
// Usage: #struct
// Returns the total number of fields in the structure definition.

static int struct_len(lua_State *Lua)
{
   auto fs = (struct fstruct *)lua_touserdata(Lua, 1);
   if (!fs) {
      luaL_argerror(Lua, 1, "Expected struct.");
      return 0;
   }
   else {
      lua_pushnumber(Lua, fs->Def->Total);
      return 1;
   }
}

//****************************************************************************
// Internal: Struct index call

static int struct_get(lua_State *Lua)
{
   auto fs = (struct fstruct *)lua_touserdata(Lua, 1);

   if (fs) {
      CSTRING fieldname;
      if ((fieldname = luaL_checkstring(Lua, 2))) {
         if (!StrCompare("structsize", fieldname, 0, STR_MATCH_CASE)) {
            lua_pushvalue(Lua, 1);
            lua_pushcclosure(Lua, &struct_structsize, 1);
            return 1;
         }
         else return get_fieldvalue(Lua, fs, fieldname);
      }
   }

   return 0;
}

//****************************************************************************

static LONG get_fieldvalue(lua_State *Lua, struct fstruct *FS, CSTRING fieldname)
{
   LONG offset, type, array_size;
   CSTRING structname = NULL;

   if ((find_field(FS, fieldname, &structname, &offset, &type, &array_size))) {
      if (!FS->Data) {
         luaL_error(Lua, "Cannot reference field '%s' because struct address is NULL.", fieldname);
         return 0;
      }

      APTR address = (BYTE *)FS->Data + offset;

      if ((type & FD_STRUCT) and (type & FD_PTR) and (structname)) { // Pointer to structure
         if (((APTR *)address)[0]) {
            if (type & FD_ARRAY) { // Array of pointers to structures.
               make_array(Lua, type, structname, (APTR *)address, array_size, FALSE);
            }
            else push_struct(Lua->Script, ((APTR *)address)[0], structname, FALSE, FALSE);
         }
         else lua_pushnil(Lua);
      }
      else if (type & FD_STRUCT) { // Embedded structure
         push_struct(Lua->Script, address, structname, FALSE, FALSE);
      }
      else if (type & FD_STRING) {
         if (type & FD_ARRAY) make_array(Lua, FD_STRING, NULL, (APTR *)address, array_size, FALSE);
         else lua_pushstring(Lua, ((STRING *)address)[0]);
      }
      else if (type & FD_OBJECT) {
         push_object(Lua, ((OBJECTPTR *)address)[0]);
      }
      else if (type & FD_POINTER) {
         if (((APTR *)address)[0]) lua_pushlightuserdata(Lua, ((APTR *)address)[0]);
         else lua_pushnil(Lua);
      }
      else if (type & FD_FUNCTION) {
         lua_pushnil(Lua);
      }
      else if (type & FD_FLOAT)   {
         if (type & FD_ARRAY) make_array(Lua, FD_FLOAT, NULL, (APTR *)address, array_size, FALSE);
         else lua_pushnumber(Lua, ((FLOAT *)address)[0]);
      }
      else if (type & FD_DOUBLE) {
         if (type & FD_ARRAY) make_array(Lua, FD_DOUBLE, NULL, (APTR *)address, array_size, FALSE);
         else lua_pushnumber(Lua, ((DOUBLE *)address)[0]);
      }
      else if (type & FD_LARGE) {
         if (type & FD_ARRAY) make_array(Lua, FD_LARGE, NULL, (APTR *)address, array_size, FALSE);
         else lua_pushnumber(Lua, ((LARGE *)address)[0]);
      }
      else if (type & FD_LONG) {
         if (type & FD_ARRAY) make_array(Lua, FD_LONG, NULL, (APTR *)address, array_size, FALSE);
         else lua_pushinteger(Lua, ((LONG *)address)[0]);
      }
      else if (type & FD_WORD) {
         if (type & FD_ARRAY) make_array(Lua, FD_WORD, NULL, (APTR *)address, array_size, FALSE);
         else lua_pushinteger(Lua, ((WORD *)address)[0]);
      }
      else if (type & FD_BYTE) {
         if ((type & FD_CUSTOM) and (type & FD_ARRAY)) {
            // Character arrays are interpreted as strings.  Use 'b' instead of 'c' if this behaviour is undesirable
            lua_pushstring(Lua, (CSTRING)address);
         }
         else if (type & FD_ARRAY) make_array(Lua, FD_BYTE, NULL, (APTR *)address, array_size, FALSE);
         else lua_pushinteger(Lua, ((UBYTE *)address)[0]);
      }
      else {
         char buffer[80];
         StrFormat(buffer, sizeof(buffer), "Field '%s' does not use a supported type ($%.8x).", fieldname, type);
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

//****************************************************************************
// Usage: fstruct.field = newvalue

static int struct_set(lua_State *Lua)
{
   auto fs = (struct fstruct *)lua_touserdata(Lua, 1);

   if (fs) {
      CSTRING ref;
      if ((ref = luaL_checkstring(Lua, 2))) {
         if (!fs->Data) {
            luaL_error(Lua, "Cannot reference field '%s' because struct address is NULL.", ref);
            return 0;
         }

         CSTRING structname;
         LONG offset, type;
         if ((find_field(fs, ref, &structname, &offset, &type, NULL))) {
            parasol::Log log;
            log.trace("struct.set() %s, Offset %d, $%.8x", ref, offset, type);

            APTR address = (BYTE *)fs->Data + offset;

            if (type & FD_STRING) {
               log.trace("Strings not supported yet.");
               // In order to set strings, we'd need make a copy of the string received from
               // Lua and free it when the field changes or the structure is destroyed.

            }
            else if (type & FD_OBJECT)  ((OBJECTPTR *)address)[0] = (OBJECTPTR)lua_touserdata(Lua, 3);
            else if (type & FD_POINTER) ((APTR *)address)[0] = lua_touserdata(Lua, 3);
            else if (type & FD_FUNCTION);
            else if (type & FD_LONG)   ((LONG *)address)[0]   = lua_tointeger(Lua, 3);
            else if (type & FD_WORD)   ((WORD *)address)[0]   = lua_tointeger(Lua, 3);
            else if (type & FD_BYTE)   ((BYTE *)address)[0]   = lua_tointeger(Lua, 3);
            else if (type & FD_DOUBLE) ((DOUBLE *)address)[0] = lua_tonumber(Lua, 3);
            else if (type & FD_FLOAT)  ((FLOAT *)address)[0]  = lua_tonumber(Lua, 3);
         }
         else luaL_error(Lua, "Invalid field reference '%s'", ref);
      }
      else luaL_error(Lua, "Translation failure.");
   }

   return 0;
}

/*****************************************************************************
** Garbage collecter.
*/

static int struct_destruct(lua_State *Lua)
{
   auto fs = (fstruct *)luaL_checkudata(Lua, 1, "Fluid.struct");
   if ((fs) AND (fs->Deallocate)) {
      FreeResource(fs->Data);
      fs->Data = NULL;
   }

   return 0;
}

//****************************************************************************
// Register the fstruct interface.

static const luaL_reg structlib_functions[] = {
   { "new",   struct_new },
   { "size",  struct_size },
   { NULL, NULL }
};

static const luaL_reg structlib_methods[] = {
   { "__index",    struct_get },
   { "__newindex", struct_set },
   { "__len",      struct_len },
   { "__gc",       struct_destruct },
   { NULL, NULL }
};

void register_struct_class(lua_State *Lua)
{
   parasol::Log log(__FUNCTION__);
   log.trace("Registering struct interface.");

   luaL_newmetatable(Lua, "Fluid.struct");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable
   luaL_openlib(Lua, NULL, structlib_methods, 0);

   luaL_openlib(Lua, "struct", structlib_functions, 0);
}
