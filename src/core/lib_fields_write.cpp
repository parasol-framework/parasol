/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

-CATEGORY-
Name: Fields
-END-

*********************************************************************************************************************/

#include "defs.h"
#include <parasol/main.h>

#include <stdarg.h>
#include <stdlib.h>

#define OP_OR        0
#define OP_AND       1
#define OP_OVERWRITE 2

static ERR writeval_array(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERR writeval_flags(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERR writeval_long(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERR writeval_large(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERR writeval_double(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERR writeval_function(OBJECTPTR, Field *, LONG, CPTR , LONG);
static ERR writeval_ptr(OBJECTPTR, Field *, LONG, CPTR , LONG);

static ERR setval_large(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERR setval_pointer(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERR setval_double(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERR setval_long(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERR setval_function(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERR setval_array(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERR setval_brgb(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);
static ERR setval_unit(OBJECTPTR, Field *, LONG Flags, CPTR , LONG);

/*********************************************************************************************************************

-FUNCTION-
SetArray: Used to set array fields in objects.

The SetArray() function is used as an alternative to ~SetField() for the purpose of writing arrays to objects.

The following code segment illustrates how to write an array to an object field:

<pre>
LONG array[100];
SetArray(Object, FID_Table|TLONG, array, 100);
</pre>

An indicator of the type of the elements in the `Array` must be OR'd into the `Field` parameter.  Available field
types are listed in ~SetField().  Note that the type that you choose must be a match to the type expected for
elements in the array.

-INPUT-
obj Object:   Pointer to the target object.
fid Field:    The universal ID of the field you wish to write to, OR'd with a type indicator.
ptr Array:    Pointer to the array values.
int Elements: The number of elements listed in the `Array`.

-ERRORS-
Okay
NullArgs
FieldTypeMismatch: The specified `Field` is not an array.
UnsupportedField: The specified `Field` is not support by the `Object` class.
NoFieldAccess:    The field is read-only.

*********************************************************************************************************************/

ERR SetArray(OBJECTPTR Object, FIELD FieldID, APTR Array, LONG Elements)
{
   pf::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR::NullArgs);
   if (Elements <= 0) log.warning("Element count not specified.");

   LONG type = (FieldID>>32)|FD_ARRAY;
   FieldID = FieldID & 0xffffffff;

   if (auto field = lookup_id(Object, FieldID, &Object)) {
      if (!(field->Flags & FD_ARRAY)) return log.warning(ERR::FieldTypeMismatch);

      if ((!(field->Flags & (FD_INIT|FD_WRITE))) and (tlContext->object() != Object)) {
         if (!field->Name) log.warning("Field %s of class %s is not writeable.", FieldName(field->FieldID), Object->className());
         else log.warning("Field \"%s\" of class %s is not writeable.", field->Name, Object->className());
         return ERR::NoFieldAccess;
      }

      if ((field->Flags & FD_INIT) and (Object->initialised()) and (tlContext->object() != Object)) {
         if (!field->Name) log.warning("Field %s in class %s is init-only.", FieldName(field->FieldID), Object->className());
         else log.warning("Field \"%s\" in class %s is init-only.", field->Name, Object->className());
         return ERR::NoFieldAccess;
      }


      Object->lock();
      ERR error = field->WriteValue(Object, field, type, Array, Elements);
      Object->unlock();
      return error;
   }
   else {
      log.warning("Could not find field %s in class %s.", FieldName(FieldID), Object->className());
      return ERR::UnsupportedField;
   }
}

/*********************************************************************************************************************

-FUNCTION-
SetField: Used to set field values of objects.

The SetField() function is used to write field values to objects.

The following code segment illustrates how to write values to an object:

<pre>
SetField(Object, FID_X|TLONG, 100);
SetField(Object, FID_Statement|TSTR, "string");
</pre>

Fields are referenced as hashed UID's calculated from the C++ `strihash()` function.  The majority of field ID's are
predefined in the `parasol/system/fields.h` include file.

The type of the `Value` parameter must be OR'd into the `Field` parameter.  If the provided type does not match that of
the field, a type conversion will occur.  All numeric types are compatible with each other and strings can also be
converted to a numeric value automatically.  String and pointer types are interchangeable.

Available field types are as follows:

<types>
<type name="TLONG">A 32-bit integer value.</>
<type name="TDOUBLE">A 64-bit floating point value.</>
<type name="TSCALE">A 64-bit floating point value that represents a scaled multiplier or percentage (1.0 is equivalent to 100%).</>
<type name="TLARGE">A 64-bit integer value.</>
<type name="TPTR">A standard address space pointer.</>
<type name="TSTR">A pointer that refers to a string.</>
<type name="TFUNCTION">A pointer to a `FUNCTION` structure.</>
<type name="TUNIT">A pointer to a `Unit` structure.</>
</>

There is no requirement for the client to have a working knowledge of the target object's field configuration in
order to write information to it.

To set a field with a fixed-size array, use the ~SetArray() function.

-INPUT-
obj Object: Pointer to the target object.
fid Field:  The universal ID of the field to update, OR'd with a type indicator.
vtags Value:  The value that will be written to the field.

-ERRORS-
Okay:
Args:
UnsupportedField: The specified field is not support by the object's class.
NoFieldAccess:    The field is read-only.

*********************************************************************************************************************/

ERR SetField(OBJECTPTR Object, FIELD FieldID, ...)
{
   pf::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR::NullArgs);

   ULONG type = FieldID>>32;
   FieldID = FieldID & 0xffffffff;

   if (auto field = lookup_id(Object, FieldID, &Object)) {
      // Validation

      if ((!(field->Flags & (FD_INIT|FD_WRITE))) and (tlContext->object() != Object)) {
         if (field->Name) log.warning("%s.%s is immutable.", Object->className(), field->Name);
         else log.warning("%s.%s is immutable.", Object->className(), FieldName(field->FieldID));
         return ERR::NoFieldAccess;
      }
      else if ((field->Flags & FD_INIT) and (Object->initialised()) and (tlContext->object() != Object)) {
         if (field->Name) log.warning("%s.%s is init-only.", Object->className(), field->Name);
         else log.warning("%s.%s is init-only.", Object->className(), FieldName(field->FieldID));
         return ERR::NoFieldAccess;
      }

      Object->lock();

      ERR error;
      va_list list;
      va_start(list, FieldID);

         if (type & (FD_POINTER|FD_STRING|FD_FUNCTION|FD_UNIT)) {
            error = field->WriteValue(Object, field, type, va_arg(list, APTR), 0);
         }
         else if (type & (FD_DOUBLE|FD_FLOAT)) {
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

      va_end(list);

      Object->unlock();
      return error;
   }
   else {
      log.warning("Could not find field %s in class %s.", FieldName(FieldID), Object->className());
      return ERR::UnsupportedField;
   }
}

//********************************************************************************************************************
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

//********************************************************************************************************************
// Used by some of the SetField() range of instructions.

ERR writeval_default(OBJECTPTR Object, Field *Field, LONG flags, CPTR Data, LONG Elements)
{
   pf::Log log("WriteField");

   //log.trace("[%s:%d] Name: %s, SetValue: %c, FieldFlags: $%.8x, SrcFlags: $%.8x", Object->className(), Object->UID, Field->Name, Field->SetValue ? 'Y' : 'N', Field->Flags, flags);

   if (!flags) flags = Field->Flags;

   if (!Field->SetValue) {
      ERR error = ERR::Okay;
      if (Field->Flags & FD_ARRAY)         error = writeval_array(Object, Field, flags, Data, Elements);
      else if (Field->Flags & FD_LONG)     error = writeval_long(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_LARGE)    error = writeval_large(Object, Field, flags, Data, 0);
      else if (Field->Flags & (FD_DOUBLE|FD_FLOAT)) error = writeval_double(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_FUNCTION) error = writeval_function(Object, Field, flags, Data, 0);
      else if (Field->Flags & (FD_POINTER|FD_STRING)) error = writeval_ptr(Object, Field, flags, Data, 0);
      else log.warning("Unrecognised field flags $%.8x.", Field->Flags);

      if (error != ERR::Okay) log.warning("An error occurred writing to field %s (field type $%.8x, source type $%.8x).", Field->Name, Field->Flags, flags);
      return error;
   }
   else {
      if (Field->Flags & FD_UNIT)          return setval_unit(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_RGB)      return setval_brgb(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_ARRAY)    return setval_array(Object, Field, flags, Data, Elements);
      else if (Field->Flags & FD_FUNCTION) return setval_function(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_LONG)     return setval_long(Object, Field, flags, Data, 0);
      else if (Field->Flags & (FD_DOUBLE|FD_FLOAT))   return setval_double(Object, Field, flags, Data, 0);
      else if (Field->Flags & (FD_POINTER|FD_STRING)) return setval_pointer(Object, Field, flags, Data, 0);
      else if (Field->Flags & FD_LARGE)    return setval_large(Object, Field, flags, Data, 0);
      else return ERR::FieldTypeMismatch;
   }
}

//********************************************************************************************************************
// The writeval() functions are used as optimised calls for all cases where the client has not provided a SetValue()
// function.

static ERR writeval_array(OBJECTPTR Object, Field *Field, LONG SrcType, CPTR Source, LONG Elements)
{
   pf::Log log("WriteField");

   // Direct writing to field arrays without a SET function is only supported for the RGB type.  The client should
   // define a SET function for all other cases.

   BYTE *offset = (BYTE *)Object + Field->Offset;

   if ((SrcType & FD_STRING) and (Field->Flags & FD_RGB)) {
      if (!Source) Source = "0,0,0,0"; // A string of NULL will 'clear' the colour (the alpha value will be zero)
      else if (Field->Flags & FD_LONG) ((RGB8 *)offset)->Alpha = 255;
      else if (Field->Flags & FD_BYTE) ((RGB8 *)offset)->Alpha = 255;
      write_array((CSTRING)Source, Field->Flags, 4, offset);
      return ERR::Okay;
   }
   else if ((SrcType & FD_POINTER) and (Field->Flags & FD_RGB)) { // Presume the source is a pointer to an RGB structure
      RGB8 *rgb = (RGB8 *)Source;
      ((RGB8 *)offset)->Red   = rgb->Red;
      ((RGB8 *)offset)->Green = rgb->Green;
      ((RGB8 *)offset)->Blue  = rgb->Blue;
      ((RGB8 *)offset)->Alpha = rgb->Alpha;
      return ERR::Okay;
   }

   log.warning("Field array '%s' is poorly defined.", Field->Name);
   return ERR::Failed;
}

static ERR writeval_flags(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   pf::Log log("WriteField");
   LONG j, int32;

   // Converts flags to numeric form if the source value is a string.

   if (Flags & FD_STRING) {
      LARGE int64 = 0;

      if (auto str = (CSTRING)Data) {
         // Check if the string is a number
         for (j=0; str[j] and (str[j] >= '0') and (str[j] <= '9'); j++);
         if (!str[j]) {
            int64 = StrToInt(str);
         }
         else if (Field->Arg) {
            bool reverse = false;
            WORD op      = OP_OVERWRITE;
            while (*str) {
               if (*str IS '&')      { op = OP_AND;       str++; }
               else if (*str IS '!') { op = OP_OR;        str++; }
               else if (*str IS '^') { op = OP_OVERWRITE; str++; }
               else if (*str IS '~') { reverse = true;    str++; }
               else {
                  // Find out how long this particular flag name is
                  for (j=0; (str[j]) and (str[j] != '|'); j++);

                  if (j > 0) {
                     FieldDef *lk = (FieldDef *)Field->Arg;
                     std::string_view sv(str, j);
                     while (lk->Name) {
                        if (iequals(lk->Name, sv)) {
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

            if (reverse) int64 = ~int64;

            // Get the current flag values from the field if special ops are requested

            if (op != OP_OVERWRITE) {
               ERR error;
               LONG currentflags;
               if ((error = copy_field_to_buffer(Object, Field, FT_LONG, &currentflags, NULL, NULL)) IS ERR::Okay) {
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
         Data  = &int32;
      }
      else if (Field->Flags & FD_LARGE) {
         Flags = FD_LARGE;
         Data  = &int64;
      }
      else return ERR::SetValueNotArray;
   }

   return writeval_default(Object, Field, Flags, Data, Elements);
}

static ERR writeval_lookup(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   pf::Log log("WriteField");
   LONG int32;

   if (Flags & FD_STRING) {
      if (Data) {
         FieldDef *lookup;
         int32 = StrToInt((CSTRING)Data); // If the Data string is a number rather than a lookup, this will extract it
         if ((lookup = (FieldDef *)Field->Arg)) {
            while (lookup->Name) {
               if (iequals((CSTRING)Data, lookup->Name)) {
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

static ERR writeval_long(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   auto offset = (LONG *)((BYTE *)Object + Field->Offset);
   if (Flags & FD_LONG)        *offset = *((LONG *)Data);
   else if (Flags & FD_LARGE)  *offset = (LONG)(*((LARGE *)Data));
   else if (Flags & (FD_DOUBLE|FD_FLOAT)) *offset = F2I(*((DOUBLE *)Data));
   else if (Flags & FD_STRING) *offset = (LONG)StrToInt((STRING)Data);
   else return ERR::SetValueNotNumeric;
   return ERR::Okay;
}

static ERR writeval_large(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   auto offset = (LARGE *)((BYTE *)Object + Field->Offset);
   if (Flags & FD_LARGE)       *offset = *((LARGE *)Data);
   else if (Flags & FD_LONG)   *offset = *((LONG *)Data);
   else if (Flags & (FD_DOUBLE|FD_FLOAT)) *offset = F2I(*((DOUBLE *)Data));
   else if (Flags & FD_STRING) *offset = strtoll((STRING)Data, NULL, 0);
   else return ERR::SetValueNotNumeric;
   return ERR::Okay;
}

static ERR writeval_double(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   auto offset = (DOUBLE *)((BYTE *)Object + Field->Offset);
   if (Flags & (FD_DOUBLE|FD_FLOAT)) *offset = *((DOUBLE *)Data);
   else if (Flags & FD_LONG)   *offset = *((LONG *)Data);
   else if (Flags & FD_LARGE)  *offset = (*((LARGE *)Data));
   else if (Flags & FD_STRING) *offset = strtod((STRING)Data, NULL);
   else return ERR::SetValueNotNumeric;
   return ERR::Okay;
}

static ERR writeval_function(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   auto offset = (FUNCTION *)((BYTE *)Object + Field->Offset);
   if (Flags & FD_FUNCTION) {
      offset[0] = ((FUNCTION *)Data)[0];
   }
   else if (Flags & FD_POINTER) {
      offset[0].Type = (Data) ? CALL::STD_C : CALL::NIL;
      offset[0].Routine = (FUNCTION *)Data;
      offset[0].Context = tlContext->object();
   }
   else return ERR::SetValueNotFunction;
   return ERR::Okay;
}

static ERR writeval_ptr(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   auto offset = (APTR *)((BYTE *)Object + Field->Offset);
   if (Flags & (FD_POINTER|FD_STRING)) *offset = (void *)Data;
   else return ERR::SetValueNotPointer;
   return ERR::Okay;
}

//********************************************************************************************************************

class FieldContext : public ObjectContext {
   bool success;

   public:
   FieldContext(OBJECTPTR Object, struct Field *Field) : ObjectContext(Object, AC_SetField, NULL) {
      if ((tlContext->Field IS Field) and (tlContext->object() IS Object)) { // Detect recursion
         success = false;
         return;
      }
      else success = true;

      Object->ActionDepth++;
   }

   ~FieldContext() {
      if (success) Object->ActionDepth--;
   }
};

//********************************************************************************************************************

static ERR setval_unit(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   // Convert the value to match what the unit will accept, then call the unit field's set function.

   Unit var;
   FieldContext ctx(Object, Field);

   if (Flags & (FD_LONG|FD_LARGE)) {
      var.Type = FD_LARGE | (Flags & (~(FD_LONG|FD_LARGE|FD_DOUBLE|FD_POINTER|FD_STRING)));
      if (Flags & FD_LONG) var.Double = *((LONG *)Data);
      else var.Double = *((LARGE *)Data);
      return ((ERR (*)(APTR, Unit *))(Field->SetValue))(Object, &var);
   }
   else if (Flags & (FD_DOUBLE|FD_FLOAT)) {
      var.Type = FD_DOUBLE | (Flags & (~(FD_LONG|FD_LARGE|FD_DOUBLE|FD_POINTER|FD_STRING)));
      var.Double = *((DOUBLE *)Data);
      return ((ERR (*)(APTR, Unit *))(Field->SetValue))(Object, &var);
   }
   else if (Flags & (FD_POINTER|FD_STRING)) {
      if (Field->Flags & FD_SCALED) {
         // Percentages are only applicable to numeric variables, and require conversion in advance.
         // NB: If a field needs total control over variable conversion, it should not specify FD_SCALED.
         STRING pct;
         var.Double = strtod((CSTRING)Data, &pct);
         if (pct[0] IS '%') {
            var.Type = FD_DOUBLE|FD_SCALED;
            var.Double *= 0.01;
            return ((ERR (*)(APTR, Unit *))(Field->SetValue))(Object, &var);
         }
         else {
            var.Type = FD_DOUBLE;
            return ((ERR (*)(APTR, Unit *))(Field->SetValue))(Object, &var);
         }
      }
      else var.Double = strtod((CSTRING)Data, NULL);

      var.Type = FD_DOUBLE | (Flags & (~(FD_LONG|FD_LARGE|FD_DOUBLE|FD_POINTER))); // Allows support flags like FD_STRING to fall through
      return ((ERR (*)(APTR, Unit *))(Field->SetValue))(Object, &var);
   }
   else if (Flags & FD_UNIT) {
      return ((ERR (*)(APTR, APTR))(Field->SetValue))(Object, (APTR)Data);
   }
   else return ERR::FieldTypeMismatch;
}

static ERR setval_brgb(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   if (Field->Flags & FD_BYTE) {
      FieldContext ctx(Object, Field);

      RGB8 rgb;
      rgb.Alpha = 255;
      write_array((CSTRING)Data, FD_BYTE, 4, &rgb);
      ERR error = ((ERR (*)(APTR, RGB8 *, LONG))(Field->SetValue))(Object, &rgb, 4);

      return error;
   }
   else return ERR::FieldTypeMismatch;
}

static ERR setval_array(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   FieldContext ctx(Object, Field);

   if (Flags & FD_ARRAY) {
      // Basic type checking
      LONG src_type = Flags & (FD_LONG|FD_LARGE|FD_FLOAT|FD_DOUBLE|FD_POINTER|FD_BYTE|FD_WORD|FD_STRUCT);
      if (src_type) {
         LONG dest_type = Field->Flags & (FD_LONG|FD_LARGE|FD_FLOAT|FD_DOUBLE|FD_POINTER|FD_BYTE|FD_WORD|FD_STRUCT);
         if (!(src_type & dest_type)) return ERR::SetValueNotArray;
      }

      return ((ERR (*)(APTR, APTR, LONG))(Field->SetValue))(Object, (APTR)Data, Elements);
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

         auto error = ((ERR (*)(APTR, APTR, LONG))(Field->SetValue))(Object, arraybuffer, Elements);

         free(arraybuffer);
         return error;
      }
      else return ERR::AllocMemory;
   }
   else {
      pf::Log log(__FUNCTION__);
      log.warning("Arrays can only be set using the FD_ARRAY type.");
      return ERR::SetValueNotArray;
   }
}

static ERR setval_function(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   OBJECTPTR caller = tlContext->object();
   FieldContext ctx(Object, Field);

   if (Flags & FD_FUNCTION) {
      return ((ERR (*)(APTR, APTR))(Field->SetValue))(Object, (APTR)Data);
   }
   else if (Flags & FD_POINTER) {
      FUNCTION func;
      if (Data) {
         func.Type = CALL::STD_C;
         func.Context = caller;
         func.Routine = (APTR)Data;
      }
      else func.clear();
      return ((ERR (*)(APTR, FUNCTION *))(Field->SetValue))(Object, &func);
   }
   else return ERR::SetValueNotFunction;
}

static ERR setval_long(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   FieldContext ctx(Object, Field);

   LONG int32;
   if (Flags & FD_LARGE)       int32 = (LONG)(*((LARGE *)Data));
   else if (Flags & (FD_DOUBLE|FD_FLOAT)) int32 = F2I(*((DOUBLE *)Data));
   else if (Flags & FD_STRING) int32 = strtol((STRING)Data, NULL, 0);
   else if (Flags & FD_LONG)   int32 = *((LONG *)Data);
   else return ERR::SetValueNotNumeric;

   return ((ERR (*)(APTR, LONG))(Field->SetValue))(Object, int32);
}

static ERR setval_double(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   FieldContext ctx(Object, Field);

   DOUBLE float64;
   if (Flags & FD_LONG)        float64 = *((LONG *)Data);
   else if (Flags & FD_LARGE)  float64 = (DOUBLE)(*((LARGE *)Data));
   else if (Flags & FD_STRING) float64 = strtod((CSTRING)Data, NULL);
   else if (Flags & (FD_DOUBLE|FD_FLOAT)) float64 = *((DOUBLE *)Data);
   else return ERR::SetValueNotNumeric;

   return ((ERR (*)(APTR, DOUBLE))(Field->SetValue))(Object, float64);
}

static ERR setval_pointer(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   FieldContext ctx(Object, Field);

   if (Flags & (FD_POINTER|FD_STRING)) {
      return ((ERR (*)(APTR, CPTR ))(Field->SetValue))(Object, Data);
   }
   else if (Flags & FD_LONG) {
      char buffer[32];
      IntToStr(*((LONG *)Data), buffer, sizeof(buffer));
      return ((ERR (*)(APTR, char *))(Field->SetValue))(Object, buffer);
   }
   else if (Flags & FD_LARGE) {
      char buffer[64];
      IntToStr(*((LARGE *)Data), buffer, sizeof(buffer));
      return ((ERR (*)(APTR, char *))(Field->SetValue))(Object, buffer);
   }
   else if (Flags & (FD_DOUBLE|FD_FLOAT)) {
      char buffer[64];
      IntToStr(*((DOUBLE *)Data), buffer, sizeof(buffer));
      return ((ERR (*)(APTR, char *))(Field->SetValue))(Object, buffer);
   }
   else return ERR::SetValueNotPointer;
}

static ERR setval_large(OBJECTPTR Object, Field *Field, LONG Flags, CPTR Data, LONG Elements)
{
   LARGE int64;
   FieldContext ctx(Object, Field);

   if (Flags & FD_LONG)        int64 = *((LONG *)Data);
   else if (Flags & (FD_DOUBLE|FD_FLOAT)) int64 = F2I(*((DOUBLE *)Data));
   else if (Flags & FD_STRING) int64 = strtoll((CSTRING)Data, NULL, 0);
   else if (Flags & FD_LARGE)  int64 = *((LARGE *)Data);
   else return ERR::SetValueNotNumeric;

   return ((ERR (*)(APTR, LARGE))(Field->SetValue))(Object, int64);
}

//********************************************************************************************************************
// This routine configures WriteValue so that it uses the correct set-field function, according to the field type that
// has been defined.

void optimise_write_field(Field &Field)
{
   pf::Log log(__FUNCTION__);

   if (Field.Flags & FD_FLAGS)       Field.WriteValue = writeval_flags;
   else if (Field.Flags & FD_LOOKUP) Field.WriteValue = writeval_lookup;
   else if (!Field.SetValue) {
      if (Field.Flags & FD_ARRAY)         Field.WriteValue = writeval_array;
      else if (Field.Flags & FD_LONG)     Field.WriteValue = writeval_long;
      else if (Field.Flags & FD_LARGE)    Field.WriteValue = writeval_large;
      else if (Field.Flags & (FD_DOUBLE|FD_FLOAT)) Field.WriteValue = writeval_double;
      else if (Field.Flags & FD_FUNCTION) Field.WriteValue = writeval_function;
      else if (Field.Flags & (FD_POINTER|FD_STRING)) Field.WriteValue = writeval_ptr;
      else log.warning("Invalid field flags for %s: $%.8x.", Field.Name, Field.Flags);
   }
   else {
      if (Field.Flags & FD_UNIT)      Field.WriteValue = setval_unit;
      else if (Field.Flags & FD_RGB) {
         if (Field.Flags & FD_BYTE) Field.WriteValue = setval_brgb;
         else log.warning("Invalid field flags for %s: $%.8x.", Field.Name, Field.Flags);
      }
      else if (Field.Flags & FD_ARRAY)    Field.WriteValue = setval_array;
      else if (Field.Flags & FD_FUNCTION) Field.WriteValue = setval_function;
      else if (Field.Flags & FD_LONG)     Field.WriteValue = setval_long;
      else if (Field.Flags & (FD_DOUBLE|FD_FLOAT))   Field.WriteValue = setval_double;
      else if (Field.Flags & (FD_POINTER|FD_STRING)) Field.WriteValue = setval_pointer;
      else if (Field.Flags & FD_LARGE)    Field.WriteValue = setval_large;
      else log.warning("Invalid field flags for %s: $%.8x.", Field.Name, Field.Flags);
   }
}
