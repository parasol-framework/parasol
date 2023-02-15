/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Fields
-END-

NOTE: The GetField range of functions do not provide any context management. This means that field routines that
allocate memory will have their memory tracked back to the object that made the GetField() call.  They can overcome
this by calling SetContext() themselves.

*****************************************************************************/

#include "defs.h"
#include <parasol/main.h>

static THREADVAR char strGetField[400]; // Buffer for retrieving variable field values

//********************************************************************************************************************
// This internal function provides a fast binary search of field names via ID.

Field * lookup_id(OBJECTPTR Object, ULONG FieldID, OBJECTPTR *Target)
{
   auto mc = Object->ExtClass;
   auto field = mc->prvFields;
   *Target = Object;

   LONG floor = 0;
   LONG ceiling = mc->TotalFields;
   while (floor < ceiling) {
      LONG i = (floor + ceiling)>>1;
      if (field[i].FieldID < FieldID) floor = i + 1;
      else if (field[i].FieldID > FieldID) ceiling = i;
      else {
         while ((i > 0) and (field[i-1].FieldID IS FieldID)) i--;
         return field+i;
      }
   }

   if (mc->Flags & CLF_PROMOTE_INTEGRAL) {
      for (LONG i=0; mc->Children[i] != 0xff; i++) {
         OBJECTPTR child;
         if ((!copy_field_to_buffer(Object, mc->prvFields + mc->Children[i], FT_POINTER, &child, NULL, NULL)) and (child)) {
            auto childclass = child->ExtClass;
            field = childclass->prvFields;

            LONG floor = 0;
            LONG ceiling = childclass->TotalFields;
            while (floor < ceiling) {
               LONG j = (floor + ceiling)>>1;
               if (field[j].FieldID < FieldID) floor = j + 1;
               else if (field[j].FieldID > FieldID) ceiling = j;
               else {
                  while ((j > 0) and (field[j-1].FieldID IS FieldID)) j--;
                  *Target = child;
                  return field+j;
               }
            }
         }
      }
   }
   return 0;
}

/*********************************************************************************************************************

-FUNCTION-
FindField: Finds field descriptors for any class, by ID.

The FindField() function checks if an object supports a specified field by scanning its class descriptor for a FieldID.
If a matching field is declared, its descriptor is returned.  For example:

<pre>
if ((field = FindField(Screen, FID_Width, NULL))) {
   log.msg("The field name is \"%s\".", Field-&gt;Name);
}
</pre>

The resulting Field structure is immutable.

-INPUT-
obj Object:   The target object.
uint FieldID: The 'FID' number to lookup.
&obj Target:  (Optional) The object that represents the field is returned here (in case a field belongs to an integrated child object).

-RESULT-
struct(Field): Returns a pointer to the field descriptor, otherwise NULL if not found.
-END-

Please note that FieldID is explicitly defined as 32-bit because using the FIELD type would make it 64-bit.

*****************************************************************************/

Field * FindField(OBJECTPTR Object, ULONG FieldID, OBJECTPTR *Target) // Read-only, thread safe function.
{
   if (!Object) return NULL;

   OBJECTPTR dummy;
   if (!Target) Target = &dummy;

   /*if (Object->ClassID IS ID_METACLASS) {
      // If FindField() is called on a meta-class, the fields declared for that class will be inspected rather than
      // the metaclass itself.
      return lookup_id_byclass((extMetaClass *)Object, FieldID, (extMetaClass **)Target);
   }
   else*/ return lookup_id(Object, FieldID, Target);
}

/*********************************************************************************************************************

-FUNCTION-
GetField: Retrieves single field values from objects.

The GetField() function is used to read field values from objects.  There is no requirement for the client to have
an understanding of the target object in order to read information from it.

The following code segment illustrates how to read values from an object:

<pre>
GetField(Object, FID_X|TLONG, &x);
GetField(Object, FID_Y|TLONG, &y);
</pre>

As GetField() is based on field ID's that reflect field names ("FID's"), you will find that there are occasions where
there is no reserved ID for the field that you wish to read.  To convert field names into their relevant IDs, call
the ~StrHash() function.  Reserved field ID's are listed in the `parasol/system/fields.h` include file.

The type of the Result parameter must be OR'd into the Field parameter.  When reading a field you must give
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
Okay:             The field value was read successfully.
Args:             Invalid arguments were specified.
NoFieldAccess:    Permissions for this field indicate that it is not readable.
UnsupportedField: The Field is not supported by the object's class.

*****************************************************************************/

ERROR GetField(OBJECTPTR Object, FIELD FieldID, APTR Result)
{
   parasol::Log log(__FUNCTION__);
   if ((!Object) or (!Result)) return log.warning(ERR_NullArgs);

   ULONG type = FieldID>>32;
   FieldID = FieldID & 0xffffffff;

#ifdef _LP64
   if (type & (FD_DOUBLE|FD_LARGE|FD_POINTER|FD_STRING)) *((LARGE *)Result) = 0;
   else if (type & FD_VARIABLE); // Do not touch variable storage.
   else *((LONG *)Result)  = 0;
#else
   if (type & (FD_DOUBLE|FD_LARGE)) *((LARGE *)Result) = 0;
   else if (type & FD_VARIABLE); // Do not touch variable storage.
   else *((LONG *)Result)  = 0;
#endif

   if (auto field = lookup_id(Object, FieldID, &Object)) {
      if (!(field->Flags & FD_READ)) {
         if (!field->Name) log.warning("Illegal attempt to read field %s.", GET_FIELD_NAME(FieldID));
         else log.warning("Illegal attempt to read field %s.", field->Name);
         return ERR_NoFieldAccess;
      }

      ScopedObjectAccess objlock(Object);
      return copy_field_to_buffer(Object, field, type, Result, NULL, NULL);
   }
   else log.warning("Unsupported field %s", GET_FIELD_NAME(FieldID));

   return ERR_UnsupportedField;
}

/*********************************************************************************************************************

-FUNCTION-
GetFieldArray: Retrieves array field values from objects.

Use the GetFieldArray() function to read an array field from an object, including the length of that array.  This
supplements the ~GetField() function, which does not support returning the array length.

This function returns the array as-is with no provision for type conversion.  If the array is null terminated, it
is standard practice not to count the null terminator in the total returned by Elements.

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

*****************************************************************************/

ERROR GetFieldArray(OBJECTPTR Object, FIELD FieldID, APTR *Result, LONG *Elements)
{
   parasol::Log log(__FUNCTION__);

   if ((!Object) or (!Result) or (!Elements)) return log.warning(ERR_NullArgs);

   LONG req_type = FieldID>>32;
   FieldID = FieldID & 0xffffffff;

   *Result = NULL;

   if (auto field = lookup_id(Object, FieldID, &Object)) {
      if ((!(field->Flags & FD_READ)) or (!(field->Flags & FD_ARRAY))) {
         if (!field->Name) log.warning("Illegal attempt to read field %s.", GET_FIELD_NAME(FieldID));
         else log.warning("Illegal attempt to read field %s.", field->Name);
         return ERR_NoFieldAccess;
      }

      if (req_type) { // Perform simple type validation if requested to do so.
         if (!(req_type & field->Flags)) return log.warning(ERR_Mismatch);
      }

      ScopedObjectAccess objlock(Object);
      ERROR error = copy_field_to_buffer(Object, field, FD_POINTER, Result, NULL, Elements);
      return error;
   }
   else log.warning("Unsupported field %s", GET_FIELD_NAME(FieldID));

   return ERR_UnsupportedField;
}

/*********************************************************************************************************************

-FUNCTION-
GetFields: Retrieves multiple field values in a single function call.

This function can be used to grab the values of multiple fields in a single function call.  It is primarily
provided to give a speed increase over calling the ~GetField() function multiple times.  The arguments
passed to this function are tag-based and must be terminated with a `TAGEND` marker, as shown in the following
example:

<pre>
LONG width, height;

GetFields(screen,
   FID_Width|TLONG,  &width,
   FID_Height|TLONG, &height,
   TAGEND);
</pre>

The field ID's that you specify must be logically or'd with tag definitions that indicate the type of values that you
want to get from each field.  For instance, if want to retrieve a field in floating point format, then you must use
the `TDOUBLE` tag and supply a pointer to a `DOUBLE` variable.  Please note that failing to set the tag values correctly
can often cause a program to crash.

The recognised tag types are `TPTR`, `TSTRING`, `TLONG`, `TLARGE` and `TDOUBLE`.

If GetFields() does not return ERR_Okay, this is an indication that at least one of the field settings returned an
error code, or there was an error in the processing of the tag list.  This function will attempt to process the entire
tag list even in the event of an error.

For further information on the field retrieval process, refer to the ~GetField() function.

-INPUT-
obj Object: Pointer to the object that you want to access.
vtags Tags:  Each tag set consists of a field ID OR'd with a type flag, followed by a pointer to a matching variable type that will store the value.

-ERRORS-
Okay
NullArgs
UnsupportedField

*****************************************************************************/

ERROR GetFields(OBJECTPTR Object, ...)
{
   parasol::Log log(__FUNCTION__);
   FIELD field_id;

   if (!Object) return log.warning(ERR_NullArgs);

   // Please note that the loop runs through the entire list, even if an error occurs.  This ensures that all the field values are driven to zero.

   va_list list;
   va_start(list, Object);

   ERROR error = ERR_Okay;

   ScopedObjectAccess objlock(Object);
   while ((field_id = va_arg(list, FIELD)) != TAGEND) {
      LONG fieldflags = field_id>>32;
      field_id &= 0xffffffff;

      APTR value;
      if (!(value = va_arg(list, APTR))) {
         error = ERR_NullArgs;
         break;
      }

      OBJECTPTR target;
      if (auto field = lookup_id(Object, field_id, &target)) {
         if (!(field->Flags & FD_READ)) {
            if (!field->Name) log.warning("Field #%d is not readable.", (LONG)field_id);
            else log.warning("Field \"%s\" is not readable.", field->Name);
         }

         #ifdef _LP64
            if (fieldflags & (FD_LARGE|FD_DOUBLE|FD_PTR|FD_STRING)) *((LARGE *)value) = 0;
            else *((LONG *)value) = 0;
         #else
            if (fieldflags & (FD_LARGE|FD_DOUBLE)) *((LARGE *)value) = 0;
            else *((LONG *)value) = 0;
         #endif

         if (!error) error = copy_field_to_buffer(target, field, fieldflags, value, NULL, NULL);
      }
      else {
         log.warning("Field %s is not supported by class %s.", GET_FIELD_NAME(field_id), Object->className());
         error = ERR_UnsupportedField;
      }
   }

   va_end(list);
   return error;
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
If the test passes, a value of 1 is returned, otherwise 0.

String conversion for flag and lookup based fields is also supported (by default, integer values are returned for
these field types when no other test is applied).  This feature is enabled by prefixing the field name with a `$` symbol.
If multiple fields are set, the resulting flags will be separated with the traditional OR symbol `|`.

If the field name refers to an array, it is possible to index specific values within that array by specifying a dot
after the field name, then the index number to lookup.

To check if a string is defined (rather than retrieving the entire string content which can be time consuming), prefix
the Field name with a question mark.  A value of 1 will be returned in the Buffer if the string has a minimum length
of 1 character, otherwise a value of 0 is returned in the Buffer.

-INPUT-
obj Object: Pointer to an object.
cstr Field: The name of the field that is to be retrieved.
buf(str) Buffer: Pointer to a buffer space large enough to hold the expected field value.  If the buffer is not large enough, the result will be truncated.  A buffer of 256 bytes is considered large enough for most occasions.  For generic field reading purposes, a buffer as large as 64kb may be desired.
bufsize Size: The size of the buffer that has been provided, in bytes.

-ERRORS-
Okay:             The field was value retrieved.
Args
UnsupportedField: The requested field is not supported by the object's class.
NoFieldAccess:    Permissions for this field state that it is not readable.
Mismatch:         The field value cannot be converted into a string.
-END-

*****************************************************************************/

ERROR GetFieldVariable(OBJECTPTR Object, CSTRING FieldName, STRING Buffer, LONG BufferSize)
{
   parasol::Log log("GetVariable");

   if ((!Object) or (!FieldName) or (!Buffer) or (BufferSize < 2)) {
      return log.warning(ERR_Args);
   }

   Field *field;
   char flagref[80];
   LONG i;
   ERROR error;

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
         return ERR_NoFieldAccess;
      }

      ScopedObjectAccess objlock(Object);

      if (field->Flags & (FD_STRING|FD_ARRAY)) {
         STRING str = NULL;
         if (!(error = copy_field_to_buffer(Object, field, FD_POINTER|FD_STRING, &str, ext, NULL))) {
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
      else if (field->Flags & (FD_LONG|FD_LARGE)) {
         FieldDef *lookup;
         LARGE large;

         if (!(error = copy_field_to_buffer(Object, field, FD_LARGE, &large, ext, NULL))) {
            if ((ext) and (field->Flags & (FD_FLAGS|FD_LOOKUP))) {
               Buffer[0] = '0';
               Buffer[1] = 0;

               if ((lookup = (FieldDef *)field->Arg)) {
                  while (lookup->Name) {
                     if (!StrMatch(lookup->Name, ext)) {
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

               return ERR_Okay;
            }
            else if (strconvert) {
               if (field->Flags & FD_FLAGS) {
                  if ((lookup = (FieldDef *)field->Arg)) {
                     LONG pos = 0;
                     while (lookup->Name) {
                        if (large & lookup->Value) {
                           if ((pos) and (pos < BufferSize-1)) Buffer[pos++] = '|';
                           pos += StrCopy(lookup->Name, Buffer+pos, BufferSize-pos);
                        }
                        lookup++;
                     }
                     return ERR_Okay;
                  }
               }
               else if (field->Flags & FD_LOOKUP) {
                  if ((lookup = (FieldDef *)field->Arg)) {
                     while (lookup->Name) {
                        if (large IS lookup->Value) {
                           StrCopy(lookup->Name, Buffer, BufferSize);
                           break;
                        }
                        lookup++;
                     }
                     return ERR_Okay;
                  }
               }
            }

            if (field->Flags & FD_OBJECT) {
               Buffer[0] = '#';
               IntToStr(large, Buffer+1, BufferSize-1);
            }
            else IntToStr(large, Buffer, BufferSize);
         }
         else return error;
      }
      else if (field->Flags & FD_DOUBLE) {
         DOUBLE dbl;
         if (!(error = copy_field_to_buffer(Object, field, FD_DOUBLE, &dbl, ext, NULL))) {
            snprintf(Buffer, BufferSize, "%f", dbl);
         }
         else return error;
      }
      else if (field->Flags & (FD_INTEGRAL|FD_OBJECT)) {
         OBJECTPTR obj;
         if (!(error = copy_field_to_buffer(Object, field, FD_POINTER, &obj, ext, NULL))) {
            if (ext) {
               error = GetFieldVariable(obj, ext, Buffer, BufferSize);
               return error;
            }
            else snprintf(Buffer, BufferSize, "#%d", obj->UID);
         }
         else StrCopy("0", Buffer, BufferSize);
      }
      else {
         log.warning("Field %s is not a value that can be converted to a string.", field->Name);
         return ERR_Mismatch;
      }

      return ERR_Okay;
   }
   else {
      if (!CheckAction(Object, AC_GetVar)) {
         struct acGetVar var = {
            .Field  = FieldName, // Must use the original field name argument, not the modified fname
            .Buffer = Buffer,
            .Size   = BufferSize
         };
         if (!Action(AC_GetVar, Object, &var)) {
            return ERR_Okay;
         }
         else log.msg("Could not find field %s from object %p (%s).", FieldName, Object, Object->className());
      }
      else log.warning("Could not find field %s from object %p (%s).", FieldName, Object, Object->className());

      return ERR_UnsupportedField;
   }
}

//********************************************************************************************************************
// Used by the GetField() range of functions.

ERROR copy_field_to_buffer(OBJECTPTR Object, Field *Field, LONG DestFlags, APTR Result, CSTRING Option, LONG *TotalElements)
{
   parasol::Log log("GetField");

   //log.msg("[%s:%d] Name: %s, Flags: $%x", ((extMetaClass *)Object->Class)->Name, Object->UID, Field->Name, DestFlags);

   BYTE value[16]; // 128 bits of space
   APTR data;
   LONG array_size = -1;
   LONG srcflags = Field->Flags;

   if (!(DestFlags & (FD_VARIABLE|FD_LARGE|FD_LONG|FD_DOUBLE|FD_POINTER|FD_STRING|FD_ARRAY))) goto mismatch;

   if (srcflags & FD_VARIABLE) {
      if (!Field->GetValue) return ERR_NoFieldAccess;

      Variable var;
      ERROR error;
      ObjectContext ctx(Object, 0, Field);

      if (DestFlags & FD_VARIABLE) {
         error = Field->GetValue(Object, Result);
      }
      else if (srcflags & FD_DOUBLE) {
         var.Type = FD_DOUBLE | (DestFlags & (~(FD_LONG|FD_LARGE)));
         error = Field->GetValue(Object, &var);

         if (!error) {
            if (DestFlags & FD_LARGE)       *((LARGE *)Result)  = var.Double;
            else if (DestFlags & FD_LONG)   *((LONG *)Result)   = var.Double;
            else if (DestFlags & FD_DOUBLE) *((DOUBLE *)Result) = var.Double;
            else error = ERR_FieldTypeMismatch;
         }
      }
      else if (srcflags & (FD_LARGE|FD_LONG)) {
         var.Type = FD_LARGE | (DestFlags & (~(FD_LARGE|FD_LONG|FD_DOUBLE)));

         error = Field->GetValue(Object, &var);
         if (!error) {
            if (DestFlags & FD_LARGE)       *((LARGE *)Result)  = var.Large;
            else if (DestFlags & FD_LONG)   *((LONG *)Result)   = var.Large;
            else if (DestFlags & FD_DOUBLE) *((DOUBLE *)Result) = var.Large;
            else error = ERR_FieldTypeMismatch;
         }
      }
      else {
         // Get field using the user's preferred format
         if (DestFlags & FD_LONG) var.Type = (DestFlags & (~FD_LONG)) | FD_LARGE;
         else var.Type = DestFlags;

         error = Field->GetValue(Object, &var);
         if (!error) {
            if (DestFlags & FD_LARGE)        *((LARGE *)Result)  = var.Large;
            else if (DestFlags & FD_LONG)    *((LONG *)Result)   = (LONG)var.Large;
            else if (DestFlags & FD_DOUBLE)  *((DOUBLE *)Result) = var.Double;
            else if (DestFlags & FD_POINTER) *((APTR *)Result)   = var.Pointer;
         }
      }

      if (error IS ERR_FieldTypeMismatch) goto mismatch;
      else return error;
   }

   if (Field->GetValue) {
      ObjectContext ctx(Object, 0, Field);
      ERROR (*get_field)(APTR, APTR, LONG *) = (ERROR (*)(APTR, APTR, LONG *))Field->GetValue;
      ERROR error = get_field(Object, value, &array_size);
      if (error) return error;
      data = value;
   }
   else data = ((BYTE *)Object) + Field->Offset;

   // Write the data to the result area using some basic conversion code

   if (srcflags & FD_ARRAY) {
      if (array_size IS -1) {
         log.warning("Array sizing not supported for field %s", Field->Name);
         return ERR_Failed;
      }

      if (TotalElements) *TotalElements = array_size;

      if (Option) { // If an option is specified, treat it as an array index.
         LONG index = StrToInt(Option);
         if ((index >= 0) and (index < array_size)) {
            if (srcflags & FD_LONG) data = (BYTE *)data + (sizeof(LONG) * index);
            else if (srcflags & (FD_LARGE|FD_DOUBLE))   data = (BYTE *)data + (sizeof(DOUBLE) * index);
            else if (srcflags & (FD_POINTER|FD_STRING)) data = (BYTE *)data + (sizeof(APTR) * index);
            else goto mismatch;
            // Drop through to field value conversion
         }
         else return ERR_OutOfRange;
      }
      else if (DestFlags & FD_STRING) {
         // Special feature: If a string is requested, the array values are converted to CSV format.
         LONG pos = 0;
         if (srcflags & FD_LONG) {
            LONG *array = (LONG *)data;
            for (LONG i=0; i < array_size; i++) {
               pos += IntToStr(*array++, strGetField+pos, sizeof(strGetField)-pos);
               if (((size_t)pos < sizeof(strGetField)-2) and (i+1 < array_size)) strGetField[pos++] = ',';
            }
         }
         else if (srcflags & FD_BYTE) {
            UBYTE *array = (UBYTE *)data;
            for (LONG i=0; i < array_size; i++) {
               pos += IntToStr(*array++, strGetField+pos, sizeof(strGetField)-pos);
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
      return ERR_Okay;
   }

   if (srcflags & FD_LONG) {
      if (DestFlags & FD_DOUBLE) *((DOUBLE *)Result) = *((LONG *)data);
      else if (DestFlags & FD_LONG)   *((LONG *)Result)   = *((LONG *)data);
      else if (DestFlags & FD_LARGE)  *((LARGE *)Result)  = *((LONG *)data);
      else if (DestFlags & FD_STRING) {
         if (srcflags & FD_LOOKUP) {
            FieldDef *lookup;
            // Reading a lookup field as a string is permissible, we just return the string registered in the lookup table
            if ((lookup = (FieldDef *)Field->Arg)) {
               LONG value = ((LONG *)data)[0];
               while (lookup->Name) {
                  if (value IS lookup->Value) {
                     *((CSTRING *)Result) = lookup->Name;
                     return ERR_Okay;
                  }
                  lookup++;
               }
            }
            *((STRING *)Result) = NULL;
         }
         else {
            IntToStr(*((LONG *)data), strGetField, sizeof(strGetField));
            *((STRING *)Result) = strGetField;
         }
      }
      else goto mismatch;
   }
   else if (srcflags & FD_LARGE) {
      if (DestFlags & FD_DOUBLE)      *((DOUBLE *)Result) = (DOUBLE)(*((LARGE *)data));
      else if (DestFlags & FD_LONG)   *((LONG *)Result)   = (LONG)(*((LARGE *)data));
      else if (DestFlags & FD_LARGE)  *((LARGE *)Result)  = *((LARGE *)data);
      else if (DestFlags & FD_STRING) {
         IntToStr(*((LARGE *)data), strGetField, sizeof(strGetField));
         *((STRING *)Result) = strGetField;
      }
      else goto mismatch;
   }
   else if (srcflags & FD_DOUBLE) {
      if (DestFlags & FD_LONG)        *((LONG *)Result)   = F2I(*((DOUBLE *)data));
      else if (DestFlags & FD_DOUBLE) *((DOUBLE *)Result) = *((DOUBLE *)data);
      else if (DestFlags & FD_LARGE)  *((LARGE *)Result)  = F2I(*((DOUBLE *)data));
      else if (DestFlags & FD_STRING) {
         snprintf(strGetField, sizeof(strGetField), "%f", *((DOUBLE *)data));
         *((STRING *)Result) = strGetField;
      }
      else goto mismatch;
   }
   else if (srcflags & (FD_POINTER|FD_STRING)) {
      if (DestFlags & (FD_POINTER|FD_STRING)) *((APTR *)Result) = *((APTR *)data);
      else if (srcflags & (FD_INTEGRAL|FD_OBJECT)) {
         if (auto object = *((OBJECTPTR *)data)) {
            if (DestFlags & FD_LONG)       *((LONG *)Result)  = object->UID;
            else if (DestFlags & FD_LARGE) *((LARGE *)Result) = object->UID;
            else goto mismatch;
         }
         else goto mismatch;
      }
      else goto mismatch;
   }
   else {
      log.warning("I dont recognise field flags of $%.8x.", srcflags);
      return ERR_UnrecognisedFieldType;
   }

   return ERR_Okay;

mismatch:
   log.warning("Mismatch while reading %s.%s (field $%.8x, requested $%.8x).", Object->className(), Field->Name, Field->Flags, DestFlags);
   return ERR_FieldTypeMismatch;
}
