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

Field * lookup_id(OBJECTPTR Object, uint32_t FieldID, OBJECTPTR *Target)
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
      OBJECTPTR child = *((OBJECTPTR *)(((int8_t *)Object) + mc->FieldLookup[mc->Local[i]].Offset));
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

   return nullptr;
}

/*********************************************************************************************************************

-FUNCTION-
FieldName: Resolves a field ID to its registered name.

Resolves a field identifier to its name by checking the internal dictionary.  The field must have previously been
referenced by a class blueprint in order for its name to be registered.

If `FieldID` is not registered, the value is returned as a printable hex string.

-INPUT-
uint FieldID: The unique field hash to resolve.

-RESULT-
cstr: The name of the field is returned.

*********************************************************************************************************************/

extern THREADVAR char tlFieldName[10]; // $12345678\0

CSTRING FieldName(uint32_t FieldID)
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
Field * FindField(OBJECTPTR Object, uint32_t FieldID, OBJECTPTR *Target) // Read-only, thread safe function.
{
   OBJECTPTR dummy;
   if (!Target) Target = &dummy;
   return lookup_id(Object, FieldID, Target);
}

/*********************************************************************************************************************

-FUNCTION-
GetFieldArray: Retrieves array field values from objects.

Use the GetFieldArray() function to read an array field from an object, including the length of that array.  This
supplements the ~GetField() function, which does not support returning the array length.

This function returns the array as-is with no provision for type conversion.  If the array is null terminated, it
is standard practice not to count the null terminator in the total returned by `Elements`.

To achieve a minimum level of type safety, the anticipated type of array values can be specified by
OR'ing a field type with the field identifier, e.g. `TINT` or `TSTR`.  If no type is incorporated then a check will
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

ERR GetFieldArray(OBJECTPTR Object, FIELD FieldID, APTR *Result, int *Elements)
{
   pf::Log log(__FUNCTION__);

   if ((!Object) or (!Result) or (!Elements)) return log.warning(ERR::NullArgs);

   int req_type = FieldID>>32;
   FieldID = FieldID & 0xffffffff;

   *Result = nullptr;

   if (auto field = lookup_id(Object, FieldID, &Object)) {
      if ((!field->readable()) or (!(field->Flags & FD_ARRAY))) {
         return ERR::NoFieldAccess;
      }

      if ((req_type) and (!(req_type & field->Flags))) return log.warning(ERR::FieldTypeMismatch);

      ScopedObjectAccess objlock(Object);

      int8_t value[16]; // 128 bits of space
      APTR data;
      int array_size = -1;
      auto srcflags = field->Flags;

      if (field->GetValue) {
         ObjectContext ctx(Object, AC::NIL, field);
         auto get_field = (ERR (*)(APTR, APTR, int &))field->GetValue;
         if (auto error = get_field(Object, value, array_size); error != ERR::Okay) return error;
         data = value;
      }
      else data = ((int8_t *)Object) + field->Offset;

      if (srcflags & FD_CPP) {
         *((APTR *)Result) = *((APTR *)data);
      }
      else {
         if (array_size IS -1) {
            log.warning("Array sizing not supported for field %s", field->Name);
            return ERR::Failed;
         }

         *Elements = array_size;
         *((APTR *)Result) = *((APTR *)data);
      }

      return ERR::Okay;
   }
   else log.warning("Unsupported field %s", FieldName(FieldID));

   return ERR::UnsupportedField;
}
