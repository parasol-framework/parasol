/*****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Fields
-END-

*****************************************************************************/

#include "defs.h"
#include <parasol/main.h>

#include <stdarg.h>
#include <stdlib.h>

#define OP_OR        0
#define OP_AND       1
#define OP_OVERWRITE 2

static ERROR writeval_array(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERROR writeval_flags(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERROR writeval_long(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERROR writeval_large(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERROR writeval_double(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERROR writeval_function(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERROR writeval_ptr(OBJECTPTR, Field *, LONG, CPTR , LONG);

static ERROR setval_large(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERROR setval_pointer(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERROR setval_double(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERROR setval_long(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERROR setval_function(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERROR setval_array(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERROR setval_brgb(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERROR setval_variable(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);

/*****************************************************************************

-FUNCTION-
SetArray: Used to set array fields in objects.

The SetArray() function is used as an alternative to ~SetField() for the purpose of writing arrays to objects.

The following code segment illustrates how to write an array to an object field:

<pre>
LONG array[100];
SetArray(Object, FID_Table|TLONG, array, 100);
</pre>

An indicator of the type of the elements in the Array must be OR'd into the Field parameter.  Available field types are
listed in ~SetField().  Note that the type that you choose must be a match to the type expected for elements
in the array.

-INPUT-
obj Object:   Pointer to the target object.
fid Field:    The universal ID of the field you wish to write to, OR'd with a type indicator.
ptr Array:    Pointer to the array values.
int Elements: The number of elements listed in the Array.

-ERRORS-
Okay
NullArgs
FieldTypeMismatch
UnsupportedField: The specified field is not support by the object's class.
NoFieldAccess:    The field is read-only.

*****************************************************************************/

ERROR SetArray(OBJECTPTR Object, FIELD FieldID, APTR Array, LONG Elements)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR_NullArgs);
   if (Elements <= 0) log.warning("Element count not specified.");

   LONG type = (FieldID>>32)|FD_ARRAY;
   FieldID = FieldID & 0xffffffff;

   Field *field;
   if ((field = lookup_id(Object, FieldID, &Object))) {
      if (!(field->Flags & FD_ARRAY)) return log.warning(ERR_FieldTypeMismatch);

      if ((!(field->Flags & (FD_INIT|FD_WRITE))) and (tlContext->Object != Object)) {
         if (!field->Name) log.warning("Field %s of class %s is not writeable.", GET_FIELD_NAME(field->FieldID), ((rkMetaClass *)Object->Class)->ClassName);
         else log.warning("Field \"%s\" of class %s is not writeable.", field->Name, ((rkMetaClass *)Object->Class)->ClassName);
         return ERR_NoFieldAccess;
      }

      if ((field->Flags & FD_INIT) and (Object->Flags & NF_INITIALISED) and (tlContext->Object != Object)) {
         if (!field->Name) log.warning("Field %s in class %s is init-only.", GET_FIELD_NAME(field->FieldID), ((rkMetaClass *)Object->Class)->ClassName);
         else log.warning("Field \"%s\" in class %s is init-only.", field->Name, ((rkMetaClass *)Object->Class)->ClassName);
         return ERR_NoFieldAccess;
      }


      prv_access(Object);
      ERROR error = field->WriteValue(Object, field, type, Array, Elements);
      prv_release(Object);
      return error;
   }
   else {
      log.warning("Could not find field %s in object class %s.", GET_FIELD_NAME(FieldID), ((rkMetaClass *)Object->Class)->ClassName);
      return ERR_UnsupportedField;
   }
}

/*****************************************************************************

-FUNCTION-
SetField: Used to set field values of objects.

The SetField() function is used to write field values to objects.

The following code segment illustrates how to write values to an object:

<pre>
SetField(Object, FID_X|TLONG, 100);
SetField(Object, FID_Statement|TSTR, "string");
</pre>

Fields are referenced by unique ID's that reflect their names.  On occasion you may find that there is no reserved ID
for the field that you wish to access.  To convert field names into their relevant IDs, call the
~StrHash() function.  Reserved field ID's are listed in the `parasol/system/fields.h` include file.

The type of the Value parameter must be OR'd into the Field parameter. When writing a field you must give
consideration to the type of the target, in order to prevent a type mismatch from occurring.  All numeric types are
compatible with each other and strings can also be converted to numeric types automatically.  String and pointer types
are interchangeable.

Available field types are as follows:

<types>
<type name="TLONG">A 32-bit integer value.</>
<type name="TDOUBLE">A 64-bit floating point value.</>
<type name="TLARGE">A 64-bit integer value.</>
<type name="TPTR">A standard 32-bit address space pointer.</>
<type name="TSTR">A 32-bit pointer that refers to a string.</>
</>

There is no requirement for you to have a working knowledge of the target object's field configuration in order to
write information to it.

To set a field with a fixed-size array, please use the ~SetArray() function.

-INPUT-
obj Object: Pointer to the target object.
fid Field:  The universal ID of the field to update, OR'd with a type indicator.
vtags Value:  The value that will be written to the field.

-ERRORS-
Okay:
Args:
UnsupportedField: The specified field is not support by the object's class.
NoFieldAccess:    The field is read-only.

*****************************************************************************/

ERROR SetField(OBJECTPTR Object, FIELD FieldID, ...)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR_NullArgs);

   ULONG type = FieldID>>32;
   FieldID = FieldID & 0xffffffff;

   ERROR error;
   Field *field;
   if ((field = lookup_id(Object, FieldID, &Object))) {
      // Validation

      if ((!(field->Flags & (FD_INIT|FD_WRITE))) and (tlContext->Object != Object)) {
         if (!field->Name) log.warning("Field %s of class %s is not writeable.", GET_FIELD_NAME(field->FieldID), ((rkMetaClass *)Object->Class)->ClassName);
         else log.warning("Field \"%s\" of class %s is not writeable.", field->Name, ((rkMetaClass *)Object->Class)->ClassName);
         return ERR_NoFieldAccess;
      }
      else if ((field->Flags & FD_INIT) and (Object->Flags & NF_INITIALISED) and (tlContext->Object != Object)) {
         if (!field->Name) log.warning("Field %s in class %s is init-only.", GET_FIELD_NAME(field->FieldID), ((rkMetaClass *)Object->Class)->ClassName);
         else log.warning("Field \"%s\" in class %s is init-only.", field->Name, ((rkMetaClass *)Object->Class)->ClassName);
         return ERR_NoFieldAccess;
      }

      prv_access(Object);

      va_list list;
      va_start(list, FieldID);

         if (type & (FD_POINTER|FD_STRING|FD_FUNCTION|FD_VARIABLE)) {
            error = field->WriteValue(Object, field, type, va_arg(list, APTR), 0);
         }
         else {
            if (type & FD_DOUBLE) {
               DOUBLE value = va_arg(list, DOUBLE);
               error = field->WriteValue(Object, field, type, &value, 1);
            }
            else if (type & FD_LARGE) {
               LARGE value = va_arg(list, LARGE);
               error = field->WriteValue(Object, field, type, &value, 1);
            }
            else {
               LONG value = va_arg(list, LONG);
               error = field->WriteValue(Object, field, type, &value, 1);
            }
         }

      va_end(list);

      prv_release(Object);
   }
   else {
      log.warning("Could not find field %s in object class %s.", GET_FIELD_NAME(FieldID), ((rkMetaClass *)Object->Class)->ClassName);
      error = ERR_UnsupportedField;
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
SetFields: Sets multiple field values in an object.

This function can be used to set the values of more than one field in a single function call, by using tags.  It is
provided for the purpose of giving a speed increase over calling the ~SetField() function multiple times.

The tags that you pass to this function must be arranged in a format of field ID's and values.  The following
example illustrates:

<pre>
SetFields(Surface,
   FID_Name|TSTR,    "MySurface",
   FID_Width|TLONG,  50,
   FID_Height|TLONG, 100,
   FID_X|TDOUBLE,    86.5,
   FID_Y|TDOUBLE,    40.0,
   TAGEND);
</pre>

The field ID's that you specify must be logically or'd with tag definitions that indicate the type of values that you
have set for each field.  For instance, if you set a floating point value for a field, then you must use the `TDOUBLE`
tag so that the SetFields() function can interpret the paired value correctly.  Please note that failing to set the
tag values correctly can cause a program to crash.

The recognised tag types are `TPTR`, `TSTRING`, `TLONG`, `TLARGE`, `TFUNCTION` and `TDOUBLE`.  There is a special type,
`TVAR`, which will divert the field setting through the #SetVar() action.

If this function fails, it should be assumed that all of the field settings failed and we recommend that your routine
aborts.  This function makes no attempt to 'salvage' any other fields that may be left in the list or undo any
previously successful field settings.

-INPUT-
obj Object:  Pointer to the object that you want to access.
vtags Tags: Each tag set consists of a field ID OR'd with a type flag, followed by a variable that matches the indicated type.

-ERRORS-
Okay:
NullArgs:
UnsupportedField: One of the fields is not supported by the target object.
Failed: A field setting failed due to an unspecified error.

*****************************************************************************/

ERROR SetFields(OBJECTPTR Object, ...)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR_NullArgs);

   va_list list;
   va_start(list, Object);
   ERROR error = SetFieldsF(Object, list);
   va_end(list);
   return error;
}

ERROR SetFieldsF(OBJECTPTR Object, va_list List)
{
   if (!Object) return ERR_NullArgs;

   parasol::Log log("SetFields");

   prv_access(Object);

   FIELD field_id;
   while ((field_id = va_arg(List, LARGE)) != TAGEND) {
      LONG flags = field_id>>32;

      Field *field;
      OBJECTPTR source;
      if ((field = lookup_id(Object, (ULONG)field_id, &source))) {
         // Validation checks

         if ((!(field->Flags & (FD_INIT|FD_WRITE))) and (tlContext->Object != Object)) {
            if (!field->Name) log.warning("Field %s of class %s is not writeable.", GET_FIELD_NAME(field->FieldID), ((rkMetaClass *)Object->Class)->ClassName);
            else log.warning("Field \"%s\" of class %s is not writeable.", field->Name, ((rkMetaClass *)Object->Class)->ClassName);

            if (flags & (FD_DOUBLE|FD_LARGE|FD_PTR64)) va_arg(List, LARGE);
            #ifdef _LP64
            else if (flags & FD_PTR) va_arg(List, APTR);
            #endif
            else va_arg(List, LONG);
            continue;
         }
         else if ((field->Flags & FD_INIT) and (Object->Flags & NF_INITIALISED) and (tlContext->Object != Object)) {
            if (!field->Name) log.warning("Field %s of class %s is init-only.", GET_FIELD_NAME(field->FieldID), ((rkMetaClass *)Object->Class)->ClassName);
            else log.warning("Field \"%s\" of class %s is init-only.", field->Name, ((rkMetaClass *)Object->Class)->ClassName);

            if (flags & (FD_DOUBLE|FD_LARGE|FD_PTR64)) va_arg(List, LARGE);
            #ifdef _LP64
            else if (flags & FD_PTR) va_arg(List, APTR);
            #endif
            else va_arg(List, LONG);
            continue;
         }

         if (!flags) flags = field->Flags;

         ERROR error;
         if (flags & (FD_POINTER|FD_ARRAY|FD_STRING|FD_FUNCTION)) {
            error = field->WriteValue(source, field, flags, va_arg(List, APTR), 0);
         }
         else if (flags & FD_DOUBLE) {
            DOUBLE value = va_arg(List, DOUBLE);
            error = field->WriteValue(source, field, flags, &value, 1);
         }
         else if (flags & (FD_LARGE|FD_PTR64)) {
            LARGE value = va_arg(List, LARGE);
            error = field->WriteValue(source, field, flags, &value, 1);
         }
         else {
            LONG value = va_arg(List, LONG);
            error = field->WriteValue(source, field, flags, &value, 1);
         }

         if ((error) and (error != ERR_NoSupport)) {
            log.warning("(%s:%d) Failed to set field %s (error #%d).", ((rkMetaClass *)source->Class)->ClassName, source->UniqueID, GET_FIELD_NAME(field_id), error);
            prv_release(Object);
            return error;
         }
      }
      else {
         log.warning("Field %s is not supported by class %s.", GET_FIELD_NAME(field_id), ((rkMetaClass *)Object->Class)->ClassName);
         prv_release(Object);
         return ERR_UnsupportedField;
      }
   }

   prv_release(Object);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SetFieldsID: Sets multiple field values in an object, using messages.

This function can be used to set the values of more than one field in a single function call, by using tags.  It is
provided for the purpose of giving a speed increase over calling the ~SetField() function multiple times.

The overall behaviour of this function is identical to the ~SetFields() function, with the exception that it
uses object ID's and will use the messaging system to set the field values of foreign objects.

This function does not block by default when messaging foreign objects, so error messages will not be received in the
event that a setting operation has failed.

-INPUT-
oid Object: A reference to the target object.
vtags Tags:   Group field ID's with the values that you want to set in order to create valid groups of tags.  The list must be terminated with TAGEND.

-ERRORS-
Okay
NullArgs
AccessObject
UnsupportedField: One of the fields is not supported by the target object.
Failed:           A field setting failed due to an unspecified error.

*****************************************************************************/

ERROR SetFieldsID(OBJECTID ObjectID, ...)
{
   if (!ObjectID) return ERR_NullArgs;

   OBJECTPTR object;
   if (!AccessObject(ObjectID, 3000, &object)) {
      va_list list;
      va_start(list, ObjectID);
      ERROR error = SetFieldsF(object, list);
      va_end(list);
      ReleaseObject(object);
      return error;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FUNCTION-
SetFieldEval: Sets any field using an abstract string value that is evaluated at runtime.

The SetFieldEval() function is used to set field values using JIT value abstraction.  It is both a simple a powerful
means of setting field values, at a cost of being the least efficient way of doing so.  It should only be used in
situations where speed is trumped by convenience.  It is commonly used by script languages and other types of batch
processing routines that do not prioritise speed.

This function includes an analysis feature that will convert named flags and lookups to their correct numeric values.
For example, setting the Flags field of a surface object with `STICKY|MASK` will result in the references being
converted to the correct hexadecimal value.

Setting object typed fields also enables special support for the commands `self`, `owner` and ID values such as
`#14592`.  In all other cases the Value string is considered to refer to an object's name.  The `self` keyword
will translate to the object specified by the Object argument, and Owner will likewise translate to the owner of that
Object.

Variable field names are supported, but the field name must be prefixed by the `@` symbol to ensure that it is not
mistaken for a true field name.

-INPUT-
obj Object: Pointer to the object to be accessed.
cstr Field: The name of the field that you want to set.
cstr Value: A string value to be written to the field.

-ERRORS-
Okay
Search: The named field does not exist in the object.
NullArgs
AccessObject
NoFieldAccess
FieldTypeMismatch
UnrecognisedFieldType
-END-

*****************************************************************************/

ERROR SetFieldEval(OBJECTPTR Object, CSTRING FieldName, CSTRING Value)
{
   parasol::Log log("WriteField");

   if ((!Object) or (!FieldName) or (!Value)) return ERR_NullArgs;

   UBYTE unlisted;
   if (*FieldName IS '@') {
      unlisted = TRUE;
      FieldName++;
   }
   else unlisted = FALSE;

   LONG i;
   ULONG hash = 5381;
   for (i=0; FieldName[i]; i++) {
      char c = FieldName[i];

      if (c IS '.') {
         FieldName += i + 1;
         ERROR error;
         OBJECTPTR child;
         if (((error = GetField(Object, hash|TPTR, &child)) != ERR_Okay) or (!child)) {
            if (error IS ERR_FieldTypeMismatch) {
               // The object reference might be an ID
               OBJECTID object_id;
               if ((!(error = GetField(Object, hash|TLONG, &object_id))) and (object_id)) {
                  if (!AccessObject(object_id, 3000, &Object)) {
                     error = SetFieldEval(Object, FieldName, Value);
                     ReleaseObject(Object);
                     return error;
                  }
                  else return ERR_AccessObject;
               }
            }

            return ERR_Search;
         }

         Object = child;
         hash = 5381;
         i = -1;
      }
      else {
         if ((c >= 'A') and (c <= 'Z')) c = c - 'A' + 'a';
         hash = ((hash<<5) + hash) + c;
      }
   }

   Field *Field;
   if ((unlisted) or (!(Field = lookup_id(Object, hash, &Object)))) {
      // If the field does not exist, check if the class supports the SetVar action.

      if (!CheckAction(Object, AC_SetVar)) {
         struct acSetVar var = { .Field = FieldName, .Value = Value };
         return Action(AC_SetVar, Object, &var);
      }
      else log.warning("Object %d (%s) does not support field '%s' or variable fields.", Object->UniqueID, ((rkMetaClass *)Object->Class)->ClassName, FieldName);

      return ERR_Search;
   }

   if ((!(Field->Flags & (FD_INIT|FD_WRITE))) and (tlContext->Object != Object)) {
      log.warning("Field \"%s\" of class %s is not writable.", FieldName, ((rkMetaClass *)Object->Class)->ClassName);
      return ERR_NoFieldAccess;
   }

   if ((Field->Flags & FD_INIT) and (Object->Flags & NF_INITIALISED) and (tlContext->Object != Object)) {
      log.warning("Field \"%s\" in class %s is init-only.", FieldName, ((rkMetaClass *)Object->Class)->ClassName);
      return ERR_NoFieldAccess;
   }

   if (!Value[0]) Value = NULL;

   ERROR error;
   prv_access(Object);
   if (Field->Flags & FD_ARRAY) { // CSV values
      if (!Value) {
         prv_release(Object);
         return ERR_NoData;
      }
      error = Field->WriteValue(Object, Field, FD_POINTER|FD_STRING, Value, 0);
   }
   else if (Field->Flags & FD_STRING) {
      if (!Value) log.debug("Warning: Sending a NULL string to field %s, class %s", Field->Name, ((rkMetaClass *)Object->Class)->ClassName);
      error = Field->WriteValue(Object, Field, FD_POINTER|FD_STRING, Value, 0);
   }
   else if (Field->Flags & FD_FUNCTION) {
      error = ERR_FieldTypeMismatch;
   }
   else if (Value) {
      if (Field->Flags & FD_DOUBLE) {
         DOUBLE dbl = StrToFloat(Value);
         for (i=0; Value[i]; i++);
         if (Value[i-1] IS '%') {
            error = Field->WriteValue(Object, Field, FD_DOUBLE|FD_PERCENTAGE, &dbl, 0);
         }
         else error = Field->WriteValue(Object, Field, FD_DOUBLE, &dbl, 0);
      }
      else if (Field->Flags & (FD_FLAGS|FD_LOOKUP)) {
         error = Field->WriteValue(Object, Field, FD_STRING, Value, 0);
      }
      else if (Field->Flags & FD_OBJECT) {
         OBJECTID object_id;

         // When setting an object field, a name can be passed as a reference to the object that the user wants to set, or
         // we may be passed an object ID, e.g. #599834.

         // If the keyword "self" is passed, it means "set the value so that it points back to me."  The special keyword
         // "owner" means just that.

         // If the string is enclosed in square brackets [], then they will be ignored.

         if (*Value IS '#')                      object_id = (LONG)StrToInt(Value+1);
         else if (!StrMatch("self", Value))      object_id = Object->UniqueID;
         else if (!StrMatch("owner", Value))     object_id = Object->OwnerID;
         else if ((!*Value) or ((Value[0] IS '0') and (!Value[1]))) object_id = 0;
         else {
            OBJECTID array[30];
            LONG count = ARRAYSIZE(array);
            if (!FindObject(Value, 0, FOF_INCLUDE_SHARED, array, &count)) {
               object_id = array[i-1];
            }
            else {
               log.warning("Object \"%s\" could not be found.", Value);
               prv_release(Object);
               return ERR_Search;
            }
         }

         if (Field->Flags & FD_LONG) {
            error = Field->WriteValue(Object, Field, FDF_OBJECTID, &object_id, 0);
         }
         else {
            OBJECTPTR target;
            if ((target = GetObjectPtr(object_id))) {
               error = Field->WriteValue(Object, Field, FDF_POINTER, target, 0);
            }
            else error = ERR_Search;
         }
      }
      else if (Field->Flags & (FD_LONG|FD_LARGE)) {
         // NB: Although placing this part of the routine at the front would be more optimal, it must be placed
         // last because fields like OBJECTID are common to LONG, and must be processed at a higher priority.

         if (Field->Flags & FD_PERCENTAGE) { // If the target field accepts percentages, we need to process the source as a double (conversion can be performed later if the target is non-variable)
            DOUBLE dbl = StrToFloat(Value);
            for (i=0; Value[i]; i++);
            if (Value[i-1] IS '%') {
               error = Field->WriteValue(Object, Field, FD_DOUBLE|FD_PERCENTAGE, &dbl, 0);
            }
            else error = Field->WriteValue(Object, Field, FD_DOUBLE, &dbl, 0);
         }
         else {
            LARGE num = StrToInt(Value);
            error = Field->WriteValue(Object, Field, FD_LARGE, &num, 0);
         }
      }
      else error = ERR_UnrecognisedFieldType;
   }
   else if (Field->Flags & FD_VARIABLE) {
      if (!Value) log.msg("Warning: Sending a NULL string to field %s, class %s", Field->Name, ((rkMetaClass *)Object->Class)->ClassName);
      error = Field->WriteValue(Object, Field, FD_POINTER|FD_STRING, Value, 0);
   }
   else error = ERR_UnrecognisedFieldType;

   prv_release(Object);
   return error;
}

//****************************************************************************
// Converts a CSV string into an array (or use "#0x123..." for a hexadecimal byte list)

static LONG write_array(CSTRING String, LONG Flags, WORD ArraySize, APTR Dest)
{
   WORD i;
   UBYTE byte;

   if (!ArraySize) ArraySize = 0x7fff; // If no ArraySize is specified then there is no imposed limit.

   if ((String[0] IS '#') or ((String[0] IS '0') and (String[1] IS 'x'))) {
      // Array is a sequence of hexadecimal bytes
      String++;
      for (i=0; i < ArraySize; i++) {
         if (*String) {
            if ((*String >= '0') and (*String <= '9')) byte = (*String - '0')<<4;
            else if ((*String >= 'A') and (*String <= 'F')) byte = ((*String - 'A')+10)<<4;
            else if ((*String >= 'a') and (*String <= 'f')) byte = ((*String - 'a')+10)<<4;
            else byte = 0;
            String++;
            if (*String) {
               if ((*String >= '0') and (*String <= '9')) byte += (*String - '0');
               else if ((*String >= 'A') and (*String <= 'F')) byte += ((*String - 'A')+10);
               else if ((*String >= 'a') and (*String <= 'f')) byte += ((*String - 'a')+10);
               String++;

               if (Flags & FD_LONG)        ((LONG *)Dest)[i]   = byte;
               else if (Flags & FD_BYTE)   ((BYTE *)Dest)[i]   = byte;
               else if (Flags & FD_FLOAT)  ((FLOAT *)Dest)[i]  = byte;
               else if (Flags & FD_DOUBLE) ((DOUBLE *)Dest)[i] = byte;
            }
         }
      }
      return i;
   }
   else {
      // Assume String is in CSV format
      if (Flags & FD_LONG) {
         for (i=0; (i < ArraySize) and (*String); i++) {
            ((LONG *)Dest)[i] = StrToInt(String);
            while ((*String > 0x20) and (*String != ',')) String++;
            if (*String) String++;
         }
         return i;
      }
      else if (Flags & FD_BYTE) {
         for (i=0; (i < ArraySize) and (*String); i++) {
            ((UBYTE *)Dest)[i] = StrToInt(String);
            while ((*String > 0x20) and (*String != ',')) String++;
            if (*String) String++;
         }
         return i;
      }
      else if (Flags & FD_FLOAT) {
         for (i=0; (i < ArraySize) and (*String); i++) {
            ((FLOAT *)Dest)[i] = StrToFloat(String);
            while ((*String > 0x20) and (*String != ',')) String++;
            if (*String) String++;
         }
         return i;
      }
      else if (Flags & FD_DOUBLE) {
         for (i=0; (i < ArraySize) and (*String); i++) {
            ((DOUBLE *)Dest)[i] = StrToFloat(String);
            while ((*String > 0x20) and (*String != ',')) String++;
            if (*String) String++;
         }
         return i;
      }
   }

   return 0;
}

//****************************************************************************
// Used by the SetField() range of instructions.

ERROR writeval_default(OBJECTPTR Object, Field *Field, LONG flags, CPTR Data, LONG Elements)
{
   parasol::Log log("WriteField");

   //log.trace("[%s:%d] Name: %s, SetValue: %c, FieldFlags: $%.8x, SrcFlags: $%.8x", ((rkMetaClass *)Object->Class)->ClassName, Object->UniqueID, Field->Name, Field->SetValue ? 'Y' : 'N', Field->Flags, flags);

   if (!flags) flags = Field->Flags;

   if (!Field->SetValue) {
      ERROR error = ERR_Okay;
      if (Field->Flags & FD_ARRAY)         error = writeval_array(Object, Field, flags, Data, Elements);
      else if (Field->Flags & FD_LONG)     error = writeval_long(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_LARGE)    error = writeval_large(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_DOUBLE)   error = writeval_double(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_FUNCTION) error = writeval_function(Object, Field, flags, Data, 0);
      else if (Field->Flags & (FD_POINTER|FD_STRING)) error = writeval_ptr(Object, Field, flags, Data, 0);
      else log.warning("Unrecognised field flags $%.8x.", Field->Flags);

      if (error != ERR_Okay) log.warning("An error occurred writing to field %s (field type $%.8x, source type $%.8x).", Field->Name, Field->Flags, flags);
      return error;
   }
   else {
      if (Field->Flags & FD_VARIABLE)      return setval_variable(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_RGB)      return setval_brgb(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_ARRAY)    return setval_array(Object, Field, flags, Data, Elements);
      else if (Field->Flags & FD_FUNCTION) return setval_function(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_LONG)     return setval_long(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_DOUBLE)   return setval_double(Object, Field, flags, Data, 0);
      else if (Field->Flags & (FD_POINTER|FD_STRING)) return setval_pointer(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_LARGE)    return setval_large(Object, Field, flags, Data, 0);
      else return ERR_FieldTypeMismatch;
   }
}

//****************************************************************************
// The writeval() functions are used as optimised calls for all cases where the client has not provided a SetValue()
// function.

static ERROR writeval_array(OBJECTPTR Object, Field *Field, LONG SrcType, CPTR Source, LONG Elements)
{
   parasol::Log log("WriteField");

   // Direct writing to field arrays without a SET function is only supported for the RGB type.  The client should
   // define a SET function for all other cases.

   BYTE *offset = (BYTE *)Object + Field->Offset;

   if ((SrcType & FD_STRING) and (Field->Flags & FD_RGB)) {
      if (!Source) Source = "0,0,0,0"; // A string of NULL will 'clear' the colour (the alpha value will be zero)
      else if (Field->Flags & FD_LONG) ((RGB8 *)offset)->Alpha = 255;
      else if (Field->Flags & FD_BYTE) ((RGB8 *)offset)->Alpha = 255;
      write_array((CSTRING)Source, Field->Flags, 4, offset);
      return ERR_Okay;
   }
   else if ((SrcType & FD_POINTER) and (Field->Flags & FD_RGB)) { // Presume the source is a pointer to an RGB structure
      RGB8 *rgb = (RGB8 *)Source;
      ((RGB8 *)offset)->Red   = rgb->Red;
      ((RGB8 *)offset)->Green = rgb->Green;
      ((RGB8 *)offset)->Blue  = rgb->Blue;
      ((RGB8 *)offset)->Alpha = rgb->Alpha;
      return ERR_Okay;
   }

   log.warning("Field array '%s' is poorly defined.", Field->Name);
   return ERR_Failed;
}

static ERROR writeval_flags(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   parasol::Log log("WriteField");
   LONG j, int32;

   // Converts flags to numeric form if the source value is a string.

   if (Flags & FD_STRING) {
      LARGE int64 = 0;

      CSTRING str;
      if ((str = (CSTRING)Data)) {
         // Check if the string is a number
         for (j=0; str[j] and (str[j] >= '0') and (str[j] <= '9'); j++);
         if (!str[j]) {
            int64 = StrToInt(str);
         }
         else if (Field->Arg) {
            WORD reverse = FALSE;
            WORD op      = OP_OVERWRITE;
            while (*str) {
               if (*str IS '&')      { op = OP_AND;       str++; }
               else if (*str IS '!') { op = OP_OR;        str++; }
               else if (*str IS '^') { op = OP_OVERWRITE; str++; }
               else if (*str IS '~') { reverse = TRUE;    str++; }
               else {
                  // Find out how long this particular flag name is
                  for (j=0; (str[j]) and (str[j] != '|'); j++);

                  if (j > 0) {
                     FieldDef *lk = (FieldDef *)Field->Arg;
                     while (lk->Name) {
                        if ((!StrCompare(lk->Name, str, j, 0)) and (!lk->Name[j])) {
                           int64 |= lk->Value;
                           break;
                        }
                        lk++;
                     }
                  }
                  str += j;
                  while (*str IS '|') str++;
               }
            }

            if (reverse IS TRUE) int64 = ~int64;

            // Get the current flag values from the field if special ops are requested

            if (op != OP_OVERWRITE) {
               ERROR error;
               LONG currentflags;
               if (!(error = copy_field_to_buffer(Object, Field, FT_LONG, &currentflags, NULL, NULL))) {
                  if (op IS OP_OR) int64 = currentflags | int64;
                  else if (op IS OP_AND) int64 = currentflags & int64;
               }
               else return error;
            }
         }
         else log.warning("Missing flag definitions for field \"%s\"", Field->Name);
      }

      if (Field->Flags & FD_LONG) {
         int32 = int64;
         Flags = FD_LONG;
         Data = &int32;
      }
      else if (Field->Flags & FD_LARGE) {
         Flags = FD_LARGE;
         Data= &int64;
      }
      else return ERR_FieldTypeMismatch;
   }

   return writeval_default(Object, Field, Flags, Data, Elements);
}

static ERROR writeval_lookup(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   parasol::Log log("WriteField");
   LONG int32;

   if (Flags & FD_STRING) {
      if (Data) {
         FieldDef *lookup;
         int32 = StrToInt((CSTRING)Data); // If the Data string is a number rather than a lookup, this will extract it
         if ((lookup = (FieldDef *)Field->Arg)) {
            while (lookup->Name) {
               if (!StrCompare((CSTRING)Data, lookup->Name, 0, STR_MATCH_LEN)) {
                  int32 = lookup->Value;
                  break;
               }
               lookup++;
            }
         }
         else log.warning("Missing lookup table definitions for field \"%s\"", Field->Name);
      }
      else int32 = 0;

      Flags = FD_LONG;
      Data  = &int32;
   }

   return writeval_default(Object, Field, Flags, Data, Elements);
}

static ERROR writeval_long(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   LONG *offset = (LONG *)((BYTE *)Object + Field->Offset);
   if (Flags & FD_LONG)        *offset = *((LONG *)Data);
   else if (Flags & FD_LARGE)  *offset = (LONG)(*((LARGE *)Data));
   else if (Flags & FD_DOUBLE) *offset = F2I(*((DOUBLE *)Data));
   else if (Flags & FD_STRING) *offset = (LONG)StrToInt((STRING)Data);
   else return ERR_FieldTypeMismatch;
   return ERR_Okay;
}

static ERROR writeval_large(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   LARGE *offset = (LARGE *)((BYTE *)Object + Field->Offset);
   if (Flags & FD_LARGE)       *offset = *((LARGE *)Data);
   else if (Flags & FD_LONG)   *offset = *((LONG *)Data);
   else if (Flags & FD_DOUBLE) *offset = F2I(*((DOUBLE *)Data));
   else if (Flags & FD_STRING) *offset = StrToInt((STRING)Data);
   else return ERR_FieldTypeMismatch;
   return ERR_Okay;
}

static ERROR writeval_double(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   DOUBLE *offset = (DOUBLE *)((BYTE *)Object + Field->Offset);
   if (Flags & FD_DOUBLE)      *offset = *((DOUBLE *)Data);
   else if (Flags & FD_LONG)   *offset = *((LONG *)Data);
   else if (Flags & FD_LARGE)  *offset = (*((LARGE *)Data));
   else if (Flags & FD_STRING) *offset = StrToFloat((STRING)Data);
   else return ERR_FieldTypeMismatch;
   return ERR_Okay;
}

static ERROR writeval_function(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   FUNCTION *offset = (FUNCTION *)((BYTE *)Object + Field->Offset);
   if (Flags & FD_FUNCTION) {
      offset[0] = ((FUNCTION *)Data)[0];
   }
   else if (Flags & FD_POINTER) {
      offset[0].Type = (Data) ? CALL_STDC : CALL_NONE;
      offset[0].StdC.Routine = (FUNCTION *)Data;
      offset[0].StdC.Context = tlContext->Object;
   }
   else return ERR_FieldTypeMismatch;
   return ERR_Okay;
}

static ERROR writeval_ptr(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   APTR *offset = (APTR *)((BYTE *)Object + Field->Offset);
   if (Flags & (FD_POINTER|FD_STRING)) *offset = (void *)Data;
   else return ERR_FieldTypeMismatch;
   return ERR_Okay;
}

//****************************************************************************

INLINE void SET_CONTEXT(OBJECTPTR Object, Field *CurrentField, ObjectContext *Context)
{
   if ((tlContext->Field IS CurrentField) and (tlContext->Object IS Object)) return; // Detect recursion

   Context->Stack  = tlContext;
   Context->Object = Object;
   Context->Field  = CurrentField;
   Context->Action = AC_SetField;

   tlContext = Context;
   Object->ActionDepth++;
}

INLINE void RESTORE_CONTEXT(OBJECTPTR Object)
{
   Object->ActionDepth--;
   tlContext = tlContext->Stack;
}

static ERROR setval_variable(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   // Convert the value to match what the variable will accept, then call the variable field's set function.

   Variable var;
   ERROR error;
   ObjectContext ctx;
   SET_CONTEXT(Object, Field, &ctx);

   if (Flags & (FD_LONG|FD_LARGE)) {
      var.Type = FD_LARGE | (Flags & (~(FD_LONG|FD_LARGE|FD_DOUBLE|FD_POINTER|FD_STRING)));
      if (Flags & FD_LONG) var.Large = *((LONG *)Data);
      else var.Large = *((LARGE *)Data);
      error = ((ERROR (*)(APTR, Variable *))(Field->SetValue))(Object, &var);
   }
   else if (Flags & FD_DOUBLE) {
      var.Type = FD_DOUBLE | (Flags & (~(FD_LONG|FD_LARGE|FD_DOUBLE|FD_POINTER|FD_STRING)));
      var.Double = *((DOUBLE *)Data);
      error = ((ERROR (*)(APTR, Variable *))(Field->SetValue))(Object, &var);
   }
   else if (Flags & (FD_POINTER|FD_STRING)) {
      var.Type = FD_POINTER | (Flags & (~(FD_LONG|FD_LARGE|FD_DOUBLE|FD_POINTER))); // Allows support flags like FD_STRING to fall through
      var.Pointer = (APTR)Data;
      error = ((ERROR (*)(APTR, Variable *))(Field->SetValue))(Object, &var);
   }
   else if (Flags & FD_VARIABLE) {
      error = ((ERROR (*)(APTR, APTR))(Field->SetValue))(Object, (APTR)Data);
   }
   else error = ERR_FieldTypeMismatch;

   RESTORE_CONTEXT(Object);
   return error;
}

static ERROR setval_brgb(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   if (Field->Flags & FD_BYTE) {
      ObjectContext ctx;
      SET_CONTEXT(Object, Field, &ctx);

      RGB8 rgb;
      rgb.Alpha = 255;
      write_array((CSTRING)Data, FD_BYTE, 4, &rgb);
      ERROR error = ((ERROR (*)(APTR, RGB8 *, LONG))(Field->SetValue))(Object, &rgb, 4);

      RESTORE_CONTEXT(Object);
      return error;
   }
   else return ERR_FieldTypeMismatch;
}

static ERROR setval_array(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   parasol::Log log(__FUNCTION__);
   ERROR error;

   ObjectContext ctx;
   SET_CONTEXT(Object, Field, &ctx);

   if (Flags & FD_ARRAY) {
      // Basic type checking
      LONG src_type = Flags & (FD_LONG|FD_LARGE|FD_FLOAT|FD_DOUBLE|FD_POINTER|FD_BYTE|FD_WORD|FD_STRUCT);
      if (src_type) {
         LONG dest_type = Field->Flags & (FD_LONG|FD_LARGE|FD_FLOAT|FD_DOUBLE|FD_POINTER|FD_BYTE|FD_WORD|FD_STRUCT);
         if (!(src_type & dest_type)) return ERR_FieldTypeMismatch;
      }

      error = ((ERROR (*)(APTR, APTR, LONG))(Field->SetValue))(Object, (APTR)Data, Elements);
   }
   else if (Flags & FD_STRING) {
      APTR arraybuffer;
      if ((arraybuffer = malloc(StrLength((CSTRING)Data) * 8))) {
         if (!Data) {
            if (Field->Flags & FD_RGB) {
               Data = "0,0,0,0"; // A string of NULL will 'clear' the colour (the alpha value will be zero)
               Elements = write_array((CSTRING)Data, Field->Flags, Field->Arg, arraybuffer);
            }
            else Elements = 0;
         }
         else if (Field->Flags & FD_RGB) {
            Elements = write_array((CSTRING)Data, Field->Flags, 4, arraybuffer);
            if (Field->Flags & FD_LONG)      ((RGB8 *)arraybuffer)->Alpha = 255;
            else if (Field->Flags & FD_BYTE) ((RGB8 *)arraybuffer)->Alpha = 255;
         }
         else Elements = write_array((CSTRING)Data, Field->Flags, 0, arraybuffer);

         error = ((ERROR (*)(APTR, APTR, LONG))(Field->SetValue))(Object, arraybuffer, Elements);

         free(arraybuffer);
      }
      else error = ERR_AllocMemory;
   }
   else {
      log.warning("Arrays can only be set using the FD_ARRAY type.");
      error = ERR_FieldTypeMismatch;
   }

   RESTORE_CONTEXT(Object);
   return error;
}

static ERROR setval_function(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   OBJECTPTR current_context = tlContext->Object;

   ObjectContext ctx;
   SET_CONTEXT(Object, Field, &ctx);

   ERROR error;
   if (Flags & FD_FUNCTION) {
      error = ((ERROR (*)(APTR, APTR))(Field->SetValue))(Object, (APTR)Data);
   }
   else if (Flags & FD_POINTER) {
      FUNCTION func;
      if (Data) {
         func.Type = CALL_STDC;
         func.StdC.Context = current_context;
         func.StdC.Routine = (APTR)Data;
      }
      else func.Type = CALL_NONE;
      error = ((ERROR (*)(APTR, FUNCTION *))(Field->SetValue))(Object, &func);
   }
   else error = ERR_FieldTypeMismatch;

   RESTORE_CONTEXT(Object);
   return error;
}

static ERROR setval_long(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   ObjectContext ctx;
   SET_CONTEXT(Object, Field, &ctx);

   LONG int32;
   ERROR error;
   if (Flags & FD_LARGE)       int32 = (LONG)(*((LARGE *)Data));
   else if (Flags & FD_DOUBLE) int32 = F2I(*((DOUBLE *)Data));
   else if (Flags & FD_STRING) int32 = StrToInt((STRING)Data);
   else if (Flags & FD_LONG)   int32 = *((LONG *)Data);
   else { RESTORE_CONTEXT(Object); return ERR_FieldTypeMismatch; }

   error = ((ERROR (*)(APTR, LONG))(Field->SetValue))(Object, int32);

   RESTORE_CONTEXT(Object);
   return error;
}

static ERROR setval_double(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   ObjectContext ctx;
   SET_CONTEXT(Object, Field, &ctx);

   DOUBLE float64;
   if (Flags & FD_LONG)        float64 = *((LONG *)Data);
   else if (Flags & FD_LARGE)  float64 = (DOUBLE)(*((LARGE *)Data));
   else if (Flags & FD_STRING) float64 = StrToFloat((CSTRING)Data);
   else if (Flags & FD_DOUBLE) float64 = *((DOUBLE *)Data);
   else { RESTORE_CONTEXT(Object); return ERR_FieldTypeMismatch; }

   ERROR error = ((ERROR (*)(APTR, DOUBLE))(Field->SetValue))(Object, float64);

   RESTORE_CONTEXT(Object);
   return error;
}

static ERROR setval_pointer(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   ERROR error;
   ObjectContext ctx;
   SET_CONTEXT(Object, Field, &ctx);

   if (Flags & (FD_POINTER|FD_STRING)) {
      error = ((ERROR (*)(APTR, CPTR ))(Field->SetValue))(Object, Data);
   }
   else if (Flags & FD_LONG) {
      char buffer[32];
      IntToStr(*((LONG *)Data), buffer, sizeof(buffer));
      error = ((ERROR (*)(APTR, char *))(Field->SetValue))(Object, buffer);
   }
   else if (Flags & FD_LARGE) {
      char buffer[64];
      IntToStr(*((LARGE *)Data), buffer, sizeof(buffer));
      error = ((ERROR (*)(APTR, char *))(Field->SetValue))(Object, buffer);
   }
   else if (Flags & FD_DOUBLE) {
      char buffer[64];
      IntToStr(*((DOUBLE *)Data), buffer, sizeof(buffer));
      error = ((ERROR (*)(APTR, char *))(Field->SetValue))(Object, buffer);
   }
   else error = ERR_FieldTypeMismatch;

   RESTORE_CONTEXT(Object);
   return error;
}

static ERROR setval_large(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   ERROR error;
   LARGE int64;
   ObjectContext ctx;
   SET_CONTEXT(Object, Field, &ctx);

   if (Flags & FD_LONG)        int64 = *((LONG *)Data);
   else if (Flags & FD_DOUBLE) int64 = F2I(*((DOUBLE *)Data));
   else if (Flags & FD_STRING) int64 = StrToInt((CSTRING)Data);
   else if (Flags & FD_LARGE)  int64 = *((LARGE *)Data);
   else { RESTORE_CONTEXT(Object); return ERR_FieldTypeMismatch; }

   error = ((ERROR (*)(APTR, LARGE))(Field->SetValue))(Object, int64);

   RESTORE_CONTEXT(Object);
   return error;
}

//****************************************************************************
// This routine configures WriteValue so that it uses the correct set-field function, according to the field type that
// has been defined.

void optimise_write_field(Field *Field)
{
   parasol::Log log("WriteField");

   if (Field->Flags & FD_FLAGS)       Field->WriteValue = writeval_flags;
   else if (Field->Flags & FD_LOOKUP) Field->WriteValue = writeval_lookup;
   else if (!Field->SetValue) {
      if (Field->Flags & FD_ARRAY)         Field->WriteValue = writeval_array;
      else if (Field->Flags & FD_LONG)     Field->WriteValue = writeval_long;
      else if (Field->Flags & FD_LARGE)    Field->WriteValue = writeval_large;
      else if (Field->Flags & FD_DOUBLE)   Field->WriteValue = writeval_double;
      else if (Field->Flags & FD_FUNCTION) Field->WriteValue = writeval_function;
      else if (Field->Flags & (FD_POINTER|FD_STRING)) Field->WriteValue = writeval_ptr;
      else log.warning("Invalid field flags for %s: $%.8x.", Field->Name, Field->Flags);
   }
   else {
      if (Field->Flags & FD_VARIABLE)      Field->WriteValue = setval_variable;
      else if (Field->Flags & FD_RGB) {
         if (Field->Flags & FD_BYTE) Field->WriteValue = setval_brgb;
         else log.warning("Invalid field flags for %s: $%.8x.", Field->Name, Field->Flags);
      }
      else if (Field->Flags & FD_ARRAY)    Field->WriteValue = setval_array;
      else if (Field->Flags & FD_FUNCTION) Field->WriteValue = setval_function;
      else if (Field->Flags & FD_LONG)     Field->WriteValue = setval_long;
      else if (Field->Flags & FD_DOUBLE)   Field->WriteValue = setval_double;
      else if (Field->Flags & (FD_POINTER|FD_STRING)) Field->WriteValue = setval_pointer;
      else if (Field->Flags & FD_LARGE)    Field->WriteValue = setval_large;
      else log.warning("Invalid field flags for %s: $%.8x.", Field->Name, Field->Flags);
   }
}
