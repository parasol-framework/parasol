/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

-CATEGORY-
Name: Fields
-END-

NOTE: The GetField range of functions do not provide any context management (which is intentional). This means that
field routines that allocate memory will have their memory tracked back to the object that made the GetField() call.
They can overcome this by calling SetContext() themselves.

*********************************************************************************************************************/

#include "defs.h"
#include <parasol/main.h>

static THREADVAR char strGetField[400]; // Buffer for retrieving variable field values

//********************************************************************************************************************
// This internal function provides a fast binary search of field names via ID.

Field * lookup_id(OBJECTPTR Object, ULONG FieldID, OBJECTPTR *Target)
{
   auto mc = Object->ExtClass;
   auto &field = mc->FieldLookup;
   *Target = Object;

   unsigned floor = 0;
   unsigned ceiling = mc->BaseCeiling;
   while (floor < ceiling) {
      auto i = (floor + ceiling)>>1;
      if (field[i].FieldID < FieldID) floor = i + 1;
      else if (field[i].FieldID > FieldID) ceiling = i;
      else return &field[i];
   }

   // Sub-class fields (located in the upper register of FieldLookup)

   if (mc->BaseCeiling < mc->FieldLookup.size()) {
      unsigned floor = mc->BaseCeiling;
      auto ceiling = mc->FieldLookup.size();
      while (floor < ceiling) {
         auto i = (floor + ceiling)>>1;
         if (field[i].FieldID < FieldID) floor = i + 1;
         else if (field[i].FieldID > FieldID) ceiling = i;
         else return &field[i];
      }
   }

   // Local object support.  NOTE: This is fallback mechanism.  The client can optimise their code by
   // directly retrieving a pointer to the local object and then reading the field value from that.

   for (unsigned i=0; mc->Local[i] != 0xff; i++) {
      OBJECTPTR child = *((OBJECTPTR *)(((BYTE *)Object) + mc->FieldLookup[mc->Local[i]].Offset));
      auto &field = child->ExtClass->FieldLookup;
      unsigned floor = 0;
      auto ceiling = child->ExtClass->BaseCeiling;
      while (floor < ceiling) {
         auto j = (floor + ceiling)>>1;
         if (field[j].FieldID < FieldID) floor = j + 1;
         else if (field[j].FieldID > FieldID) ceiling = j;
         else {
            *Target = child;
            return &field[j];
         }
      }
   }

   return NULL;
}

/*********************************************************************************************************************

-FUNCTION-
FieldName: Resolves a field ID to its registered name.

Resolves a field identifier to its name.  For this to work successfully, the field must have been registered with the
internal dictionary.  This is handled automatically when a new class is added to the system.

If the `FieldID` is not registered, the value is returned back as a hex string.  The inclusion of this feature
guarantees that an empty string will never be returned.

-INPUT-
uint FieldID: The unique field hash to resolve.

-RESULT-
cstr: The name of the field is returned.

*********************************************************************************************************************/

extern THREADVAR char tlFieldName[10]; // $12345678\0

CSTRING FieldName(ULONG FieldID)
{
   if (auto lock = std::unique_lock{glmFieldKeys, 1s}) {
      if (glFields.contains(FieldID)) return glFields[FieldID].c_str();
   }
   snprintf(tlFieldName, sizeof(tlFieldName), "$%.8x", FieldID);
   return tlFieldName;
}

/*********************************************************************************************************************

-FUNCTION-
FindField: Finds field descriptors for any class, by ID.

The FindField() function checks if an object supports a specified field by scanning its class descriptor for a `FieldID`.
If a matching field is declared, its descriptor is returned.  For example:

<pre>
if (auto field = FindField(Display, FID_Width, NULL)) {
   log.msg("The field name is \"%s\".", field-&gt;Name);
}
</pre>

The resulting !Field structure is immutable.

Note: To lookup the field definition of a @MetaClass, use the @MetaClass.FindField() method.

-INPUT-
obj Object:   The target object.
uint FieldID: The 'FID' number to lookup.
&obj Target:  (Optional) The object that represents the field is returned here (in case a field belongs to an integrated child object).

-RESULT-
struct(Field): Returns a pointer to the !Field descriptor, otherwise `NULL` if not found.
-END-

*********************************************************************************************************************/

// Please note that FieldID is explicitly defined as 32-bit because using the FIELD type would make it 64-bit.
Field * FindField(OBJECTPTR Object, ULONG FieldID, OBJECTPTR *Target) // Read-only, thread safe function.
{
   OBJECTPTR dummy;
   if (!Target) Target = &dummy;
   return lookup_id(Object, FieldID, Target);
}

/*********************************************************************************************************************

-FUNCTION-
GetField: Retrieves single field values from objects.

The GetField() function is used to read field values from objects in the safest way possible.  As long as the
requested field exists, the value can most likely be read.  It is only imperative that the requested type is
compatible with the field value itself.

The following code segment illustrates how to read values from an object:

<pre>
GetField(Object, FID_X|TLONG, &x);
GetField(Object, FID_Y|TLONG, &y);
</pre>

As GetField() is based on field ID's that reflect field names (`FID`'s), you will find that there are occasions where
there is no reserved ID for the field that you wish to read.  To convert field names into their relevant IDs, call
the C++ `strihash()` function.  Reserved field ID's are listed in the `parasol/system/fields.h` include file.

The type of the `Result` parameter must be OR'd into the `Field` parameter.  When reading a field you must give
consideration to the type of the source, in order to prevent a type mismatch from occurring.  All numeric types are
compatible with each other and strings can also be converted to numeric types automatically.  String and pointer
types are interchangeable.

Available field types are as follows:

<types>
<type name="TLONG">A 32-bit integer value.</>
<type name="TDOUBLE">A 64-bit floating point value.</>
<type name="TLARGE">A 64-bit integer value.</>
<type name="TPTR">A standard 32-bit address space pointer.</>
<type name="TSTR">A 32-bit pointer that refers to a string.</>
</table>

-INPUT-
obj Object: Pointer to an object.
fid Field:  The ID of the field to read, OR'd with a type indicator.
ptr Result: Pointer to the variable that will store the result.

-ERRORS-
Okay:             The `Field` value was read successfully.
Args:             Invalid arguments were specified.
NoFieldAccess:    Permissions for this field indicate that it is not readable.
UnsupportedField: The `Field` is not supported by the object's class.

*********************************************************************************************************************/

ERR GetField(OBJECTPTR Object, FIELD FieldID, APTR Result)
{
   pf::Log log(__FUNCTION__);
   if ((!Object) or (!Result)) return log.warning(ERR::NullArgs);

   ULONG type = FieldID>>32;
   FieldID = FieldID & 0xffffffff;

#ifdef _LP64
   if (type & (FD_DOUBLE|FD_INT64|FD_POINTER|FD_STRING)) *((LARGE *)Result) = 0;
   else if (type & FD_UNIT); // Do not touch unit storage.
   else *((LONG *)Result)  = 0;
#else
   if (type & (FD_DOUBLE|FD_INT64)) *((LARGE *)Result) = 0;
   else if (type & FD_UNIT); // Do not touch unit storage.
   else *((LONG *)Result)  = 0;
#endif

   if (auto field = lookup_id(Object, FieldID, &Object)) {
      if (!(field->Flags & FD_READ)) {
         if (!field->Name) log.warning("Illegal attempt to read field %s.", FieldName(FieldID));
         else log.warning("Illegal attempt to read field %s.", field->Name);
         return ERR::NoFieldAccess;
      }

      ScopedObjectAccess objlock(Object);
      return copy_field_to_buffer(Object, field, type, Result, NULL, NULL);
   }
   else log.warning("Unsupported field %s", FieldName(FieldID));

   return ERR::UnsupportedField;
}

/*********************************************************************************************************************

-FUNCTION-
GetFieldArray: Retrieves array field values from objects.

Use the GetFieldArray() function to read an array field from an object, including the length of that array.  This
supplements the ~GetField() function, which does not support returning the array length.

This function returns the array as-is with no provision for type conversion.  If the array is null terminated, it
is standard practice not to count the null terminator in the total returned by `Elements`.

To achieve a minimum level of type safety, the anticipated type of array values can be specified by
OR'ing a field type with the field identifier, e.g. `TLONG` or `TSTR`.  If no type is incorporated then a check will
not be performed.

-INPUT-
obj Object: Pointer to an object.
fid Field:  The ID of the field that will be read.
&ptr Result: A direct pointer to the array values will be returned in this parameter.
&int Elements: The total number of elements in the array will be returned in this parameter.

-ERRORS-
Okay:
NullArgs:
NoFieldAccess:    Permissions for this field indicate that it is not readable.
UnsupportedField: The Field is not supported by the object's class.
Mismatch

*********************************************************************************************************************/

ERR GetFieldArray(OBJECTPTR Object, FIELD FieldID, APTR *Result, LONG *Elements)
{
   pf::Log log(__FUNCTION__);

   if ((!Object) or (!Result) or (!Elements)) return log.warning(ERR::NullArgs);

   LONG req_type = FieldID>>32;
   FieldID = FieldID & 0xffffffff;

   *Result = NULL;

   if (auto field = lookup_id(Object, FieldID, &Object)) {
      if ((!(field->Flags & FD_READ)) or (!(field->Flags & FD_ARRAY))) {
         if (!field->Name) log.warning("Illegal attempt to read field %s.", FieldName(FieldID));
         else log.warning("Illegal attempt to read field %s.", field->Name);
         return ERR::NoFieldAccess;
      }

      if (req_type) { // Perform simple type validation if requested to do so.
         if (!(req_type & field->Flags)) return log.warning(ERR::Mismatch);
      }

      ScopedObjectAccess objlock(Object);
      ERR error = copy_field_to_buffer(Object, field, FD_POINTER, Result, NULL, Elements);
      return error;
   }
   else log.warning("Unsupported field %s", FieldName(FieldID));

   return ERR::UnsupportedField;
}

/*********************************************************************************************************************

-FUNCTION-
GetFieldVariable: Retrieves field values by converting them into strings.

The GetFieldVariable() function is used to retrieve the value of a field without any advance knowledge of the field's
type or details.  It also supports some advanced features such as flag name lookups and the retrieval of values from
array indexes.  Although this function is simple to use, it is the slowest of the field retrieval instructions as it
relies on string-based field names and converts all results into a string buffer that you must provide.

This function does not support pointer based fields as they cannot be translated, although an exception is made for
string field types.

If the field name refers to a flag or lookup based field type, it is possible to test if a specific flag has been set.
This is achieved by specifying a dot immediately after the field name, then the name of the flag or lookup to test.
If the test passes, a value of `1` is returned, otherwise `0`.

String conversion for flag and lookup based fields is also supported (by default, integer values are returned for
these field types when no other test is applied).  This feature is enabled by prefixing the field name with a `$` symbol.
If multiple fields are set, the resulting flags will be separated with the traditional OR symbol `|`.

If the field name refers to an array, it is possible to index specific values within that array by specifying a dot
after the field name, then the index number to lookup.

To check if a string is defined (rather than retrieving the entire string content which can be time consuming), prefix
the `Field` name with a question mark.  A value of `1` will be returned in the `Buffer` if the string has a minimum length
of `1` character, otherwise a value of `0` is returned in the Buffer.

-INPUT-
obj Object: Pointer to an object.
cstr Field: The name of the field that is to be retrieved.
buf(str) Buffer: Pointer to a buffer space large enough to hold the expected field value.  If the buffer is not large enough, the result will be truncated.  A buffer of 256 bytes is considered large enough for most occasions.  For generic field reading purposes, a buffer as large as 64kb may be desired.
bufsize Size: The size of the `Buffer` that has been provided, in bytes.

-ERRORS-
Okay:             The field was value retrieved.
Args
UnsupportedField: The requested field is not supported by the object's class.
NoFieldAccess:    Permissions for this field state that it is not readable.
Mismatch:         The field value cannot be converted into a string.
-END-

*********************************************************************************************************************/

ERR GetFieldVariable(OBJECTPTR Object, CSTRING FieldName, STRING Buffer, LONG BufferSize)
{
   pf::Log log("GetVariable");

   if ((!Object) or (!FieldName) or (!Buffer) or (BufferSize < 2)) {
      return log.warning(ERR::Args);
   }

   Field *field;
   char flagref[80];
   LONG i;
   ERR error;

   Buffer[0] = 0;
   flagref[0] = 0;
   STRING ext = NULL;
   CSTRING fname = FieldName;
   bool strconvert = false;
   bool checkdefined = false;

   // NB: The $ character can be used at the start of a field name to convert lookups and flag based fields to strings.

   while (fname[0] <= 0x40) {
      if (fname[0] IS '$') {
         strconvert = true;
         fname++;
      }
      else if (fname[0] IS '?') {
         checkdefined = true;
         fname++;
      }
      else break;
   }

   // Check for dots in the fieldname.  Flags can be tested by specifying the flag name after the fieldname.

   ULONG hash = 5381;
   for (LONG i=0; fname[i]; i++) {
      char c = fname[i];

      if (c IS '.') {
         WORD j;
         // Flagref == fieldname\0flagname\0
         for (j=0; ((size_t)j < sizeof(flagref)-1) and (fname[j]); j++) flagref[j] = fname[j];
         flagref[i] = 0; // Middle termination
         flagref[j] = 0; // End termination
         fname = flagref;
         ext = flagref + i + 1;
         break;
      }
      else {
         if ((c >= 'A') and (c <= 'Z')) c = c - 'A' + 'a';
         hash = ((hash<<5) + hash) + c;
      }
   }

   if ((field = lookup_id(Object, hash, &Object))) {
      if (!(field->Flags & FD_READ)) {
         if (!field->Name) log.warning("Illegal attempt to read field %d.", field->FieldID);
         else log.warning("Illegal attempt to read field %s.", field->Name);
         return ERR::NoFieldAccess;
      }

      ScopedObjectAccess objlock(Object);

      if (field->Flags & (FD_STRING|FD_ARRAY)) {
         STRING str = NULL;
         if ((error = copy_field_to_buffer(Object, field, FD_POINTER|FD_STRING, &str, ext, NULL)) IS ERR::Okay) {
            if (checkdefined) {
               if (field->Flags & FD_STRING) {
                  if ((str) and (str[0])) Buffer[0] = '1'; // A string needs only one char (of any kind) to be considered to be defined
                  else Buffer[0] = '0';
               }
               else Buffer[0] = '1';
               Buffer[1] = 0;
            }
            else if (str) {
               for (i=0; (str[i]) and (i < BufferSize - 1); i++) Buffer[i] = str[i];
               Buffer[i] = 0;
            }
            else Buffer[0] = 0;
         }
         else {
            Buffer[0] = 0;
            return error;
         }
      }
      else if (field->Flags & (FD_INT|FD_INT64)) {
         FieldDef *lookup;
         LARGE large;

         if ((error = copy_field_to_buffer(Object, field, FD_INT64, &large, ext, NULL)) IS ERR::Okay) {
            if ((ext) and (field->Flags & (FD_FLAGS|FD_LOOKUP))) {
               Buffer[0] = '0';
               Buffer[1] = 0;

               if ((lookup = (FieldDef *)field->Arg)) {
                  while (lookup->Name) {
                     if (iequals(lookup->Name, ext)) {
                        if (field->Flags & FD_FLAGS) {
                           if (large & lookup->Value) Buffer[0] = '1';
                        }
                        else if (large IS lookup->Value) Buffer[0] = '1';
                        break;
                     }
                     lookup++;
                  }
               }
               else log.warning("No lookup table for field '%s', class '%s'.", fname, Object->className());

               return ERR::Okay;
            }
            else if (strconvert) {
               if (field->Flags & FD_FLAGS) {
                  if ((lookup = (FieldDef *)field->Arg)) {
                     LONG pos = 0;
                     while (lookup->Name) {
                        if (large & lookup->Value) {
                           if ((pos) and (pos < BufferSize-1)) Buffer[pos++] = '|';
                           pos += strcopy(lookup->Name, Buffer+pos, BufferSize-pos);
                        }
                        lookup++;
                     }
                     return ERR::Okay;
                  }
               }
               else if (field->Flags & FD_LOOKUP) {
                  if ((lookup = (FieldDef *)field->Arg)) {
                     while (lookup->Name) {
                        if (large IS lookup->Value) {
                           strcopy(lookup->Name, Buffer, BufferSize);
                           break;
                        }
                        lookup++;
                     }
                     return ERR::Okay;
                  }
               }
            }

            if (field->Flags & FD_OBJECT) {
               Buffer[0] = '#';
               strcopy(std::to_string(large), Buffer+1, BufferSize-1);
            }
            else strcopy(std::to_string(large), Buffer, BufferSize);
         }
         else return error;
      }
      else if (field->Flags & FD_DOUBLE) {
         DOUBLE dbl;
         if ((error = copy_field_to_buffer(Object, field, FD_DOUBLE, &dbl, ext, NULL)) IS ERR::Okay) {
            snprintf(Buffer, BufferSize, "%f", dbl);
         }
         else return error;
      }
      else if (field->Flags & (FD_LOCAL|FD_OBJECT)) {
         OBJECTPTR obj;
         if ((error = copy_field_to_buffer(Object, field, FD_POINTER, &obj, ext, NULL)) IS ERR::Okay) {
            if (ext) {
               error = GetFieldVariable(obj, ext, Buffer, BufferSize);
               return error;
            }
            else snprintf(Buffer, BufferSize, "#%d", obj->UID);
         }
         else strcopy("0", Buffer, BufferSize);
      }
      else {
         log.warning("Field %s is not a value that can be converted to a string.", field->Name);
         return ERR::Mismatch;
      }

      return ERR::Okay;
   }
   else {
      if (CheckAction(Object, AC::GetKey) IS ERR::Okay) {
         struct acGetKey var = {
            FieldName, // Must use the original field name argument, not the modified fname
            Buffer,
            BufferSize
         };
         if (Action(AC::GetKey, Object, &var) IS ERR::Okay) {
            return ERR::Okay;
         }
         else log.msg("Could not find field %s from object %p (%s).", FieldName, Object, Object->className());
      }
      else log.warning("Could not find field %s from object %p (%s).", FieldName, Object, Object->className());

      return ERR::UnsupportedField;
   }
}

//********************************************************************************************************************
// Used by the GetField() range of functions.

ERR copy_field_to_buffer(OBJECTPTR Object, Field *Field, LONG DestFlags, APTR Result, CSTRING Option, LONG *TotalElements)
{
   //log.msg("[%s:%d] Name: %s, Flags: $%x", ((extMetaClass *)Object->Class)->Name, Object->UID, Field->Name, DestFlags);

   BYTE value[16]; // 128 bits of space
   APTR data;
   LONG array_size = -1;
   auto srcflags = Field->Flags;

   if (!(DestFlags & (FD_UNIT|FD_INT64|FD_INT|FD_DOUBLE|FD_POINTER|FD_STRING|FD_ARRAY))) goto mismatch;

   if (srcflags & FD_UNIT) {
      if (!Field->GetValue) return ERR::NoFieldAccess;

      Unit var;
      ERR error;
      ObjectContext ctx(Object, AC::NIL, Field);

      if (DestFlags & FD_UNIT) {
         error = Field->GetValue(Object, Result);
      }
      else if (srcflags & FD_DOUBLE) {
         var.Type = FD_DOUBLE | (DestFlags & (~(FD_INT|FD_INT64|FD_DOUBLE)));
         error = Field->GetValue(Object, &var);

         if (error IS ERR::Okay) {
            if (DestFlags & FD_INT64)       *((LARGE *)Result)  = var.Value;
            else if (DestFlags & FD_INT)   *((LONG *)Result)   = var.Value;
            else if (DestFlags & FD_DOUBLE) *((DOUBLE *)Result) = var.Value;
            else error = ERR::FieldTypeMismatch;
         }
      }
      else if (srcflags & (FD_INT64|FD_INT)) {
         var.Type = FD_DOUBLE | (DestFlags & (~(FD_INT64|FD_INT|FD_DOUBLE)));

         error = Field->GetValue(Object, &var);
         if (error IS ERR::Okay) {
            if (DestFlags & FD_INT64)       *((LARGE *)Result)  = var.Value;
            else if (DestFlags & FD_INT)   *((LONG *)Result)   = var.Value;
            else if (DestFlags & FD_DOUBLE) *((DOUBLE *)Result) = var.Value;
            else error = ERR::FieldTypeMismatch;
         }
      }
      else error = ERR::FieldTypeMismatch;

      if (error IS ERR::FieldTypeMismatch) goto mismatch;
      else return error;
   }

   if (Field->GetValue) {
      ObjectContext ctx(Object, AC::NIL, Field);
      auto get_field = (ERR (*)(APTR, APTR, LONG *))Field->GetValue;
      ERR error = get_field(Object, value, &array_size);
      if (error != ERR::Okay) return error;
      data = value;
   }
   else data = ((BYTE *)Object) + Field->Offset;

   // Write the data to the result area using some basic conversion code

   if (srcflags & FD_ARRAY) {
      if (srcflags & FD_CPP) {
         if (Option) { // If an option is specified, treat it as an array index.
            auto vec = (pf::vector<int> *)data;
            if (TotalElements) *TotalElements = vec->size();
            unsigned index = strtol(Option, NULL, 0);
            if ((index >= 0) and (index < vec->size())) {
               if (srcflags & FD_INT) data = &((pf::vector<LONG> *)data)[0][index];
               else if (srcflags & (FD_INT64|FD_DOUBLE))   data = &((pf::vector<DOUBLE> *)data)[0][index];
               else if (srcflags & (FD_POINTER|FD_STRING)) data = &((pf::vector<APTR> *)data)[0][index];
               else goto mismatch;
               // Drop through to field value conversion
            }
            else return ERR::OutOfRange;
         }
         else if (DestFlags & FD_STRING) {
            // Special feature: If a string is requested, the array values are converted to CSV format.
            std::stringstream buffer;
            if (srcflags & FD_INT) {
               auto vec = (pf::vector<LONG> *)data;
               if (TotalElements) *TotalElements = vec->size();
               for (auto &val : vec[0]) buffer << val << ',';
            }
            else if (srcflags & FD_BYTE) {
               auto vec = (pf::vector<UBYTE> *)data;
               if (TotalElements) *TotalElements = vec->size();
               for (auto &val : vec[0]) buffer << val << ',';
            }
            else if (srcflags & FD_DOUBLE) {
               auto vec = (pf::vector<DOUBLE> *)data;
               if (TotalElements) *TotalElements = vec->size();
               for (auto &val : vec[0]) buffer << val << ',';
            }
            auto i = strcopy(buffer.str(), strGetField, sizeof(strGetField));
            if (i > 0) strGetField[i-1] = 0; // Eliminate trailing comma
            *((STRING *)Result) = strGetField;
         }
         else if (DestFlags & FD_POINTER) *((APTR *)Result) = *((APTR *)data);
         else goto mismatch;
      }
      else {
         if (array_size IS -1) {
            pf::Log log("GetField");
            log.warning("Array sizing not supported for field %s", Field->Name);
            return ERR::Failed;
         }

         if (TotalElements) *TotalElements = array_size;

         if (Option) { // If an option is specified, treat it as an array index.
            LONG index = strtol(Option, NULL, 0);
            if ((index >= 0) and (index < array_size)) {
               if (srcflags & FD_INT) data = (BYTE *)data + (sizeof(LONG) * index);
               else if (srcflags & (FD_INT64|FD_DOUBLE))   data = (BYTE *)data + (sizeof(DOUBLE) * index);
               else if (srcflags & (FD_POINTER|FD_STRING)) data = (BYTE *)data + (sizeof(APTR) * index);
               else goto mismatch;
               // Drop through to field value conversion
            }
            else return ERR::OutOfRange;
         }
         else if (DestFlags & FD_STRING) {
            // Special feature: If a string is requested, the array values are converted to CSV format.
            LONG pos = 0;
            if (srcflags & FD_INT) {
               LONG *array = (LONG *)data;
               for (LONG i=0; i < array_size; i++) {
                  pos += strcopy(std::to_string(*array++), strGetField+pos, sizeof(strGetField)-pos);
                  if (((size_t)pos < sizeof(strGetField)-2) and (i+1 < array_size)) strGetField[pos++] = ',';
               }
            }
            else if (srcflags & FD_BYTE) {
               UBYTE *array = (UBYTE *)data;
               for (LONG i=0; i < array_size; i++) {
                  pos += strcopy(std::to_string(*array++), strGetField+pos, sizeof(strGetField)-pos);
                  if (((size_t)pos < sizeof(strGetField)-2) and (i+1 < array_size)) strGetField[pos++] = ',';
               }
            }
            else if (srcflags & FD_DOUBLE) {
               DOUBLE *array = (DOUBLE *)data;
               for (LONG i=0; i < array_size; i++) {
                  pos += snprintf(strGetField+pos, sizeof(strGetField)-pos, "%f", *array++);
                  if (((size_t)pos < sizeof(strGetField)-2) and (i+1 < array_size)) strGetField[pos++] = ',';
               }
               }
            strGetField[pos] = 0;
            *((STRING *)Result) = strGetField;
         }
         else if (DestFlags & FD_POINTER) *((APTR *)Result) = *((APTR *)data);
         else goto mismatch;
      }
      return ERR::Okay;
   }

   if (srcflags & FD_INT) {
      if (DestFlags & FD_DOUBLE) *((DOUBLE *)Result) = *((LONG *)data);
      else if (DestFlags & FD_INT)   *((LONG *)Result)   = *((LONG *)data);
      else if (DestFlags & FD_INT64)  *((LARGE *)Result)  = *((LONG *)data);
      else if (DestFlags & FD_STRING) {
         if (srcflags & FD_LOOKUP) {
            FieldDef *lookup;
            // Reading a lookup field as a string is permissible, we just return the string registered in the lookup table
            if ((lookup = (FieldDef *)Field->Arg)) {
               LONG value = ((LONG *)data)[0];
               while (lookup->Name) {
                  if (value IS lookup->Value) {
                     *((CSTRING *)Result) = lookup->Name;
                     return ERR::Okay;
                  }
                  lookup++;
               }
            }
            *((STRING *)Result) = NULL;
         }
         else {
            strcopy(std::to_string(*((LONG *)data)), strGetField, sizeof(strGetField));
            *((STRING *)Result) = strGetField;
         }
      }
      else goto mismatch;
   }
   else if (srcflags & FD_INT64) {
      if (DestFlags & FD_DOUBLE)      *((DOUBLE *)Result) = (DOUBLE)(*((LARGE *)data));
      else if (DestFlags & FD_INT)   *((LONG *)Result)   = (LONG)(*((LARGE *)data));
      else if (DestFlags & FD_INT64)  *((LARGE *)Result)  = *((LARGE *)data);
      else if (DestFlags & FD_STRING) {
         strcopy(std::to_string(*((LARGE *)data)), strGetField, sizeof(strGetField));
         *((STRING *)Result) = strGetField;
      }
      else goto mismatch;
   }
   else if (srcflags & FD_DOUBLE) {
      if (DestFlags & FD_INT)        *((LONG *)Result)   = F2I(*((DOUBLE *)data));
      else if (DestFlags & FD_DOUBLE) *((DOUBLE *)Result) = *((DOUBLE *)data);
      else if (DestFlags & FD_INT64)  *((LARGE *)Result)  = F2I(*((DOUBLE *)data));
      else if (DestFlags & FD_STRING) {
         snprintf(strGetField, sizeof(strGetField), "%f", *((DOUBLE *)data));
         *((STRING *)Result) = strGetField;
      }
      else goto mismatch;
   }
   else if (srcflags & (FD_POINTER|FD_STRING)) {
      if (DestFlags & (FD_POINTER|FD_STRING)) *((APTR *)Result) = *((APTR *)data);
      else if (srcflags & (FD_LOCAL|FD_OBJECT)) {
         if (auto object = *((OBJECTPTR *)data)) {
            if (DestFlags & FD_INT)       *((LONG *)Result)  = object->UID;
            else if (DestFlags & FD_INT64) *((LARGE *)Result) = object->UID;
            else goto mismatch;
         }
         else goto mismatch;
      }
      else goto mismatch;
   }
   else {
      pf::Log log("GetField");
      return log.warning(ERR::UnrecognisedFieldType);
   }

   return ERR::Okay;

mismatch:
   pf::Log log("GetField");
   log.warning("Mismatch while reading %s.%s (field $%.8x, requested $%.8x).", Object->className(), Field->Name, Field->Flags, DestFlags);
   return ERR::FieldTypeMismatch;
}
