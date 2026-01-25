// Compile-time field type lookup for object field accesses.
//
// Copyright (C) 2026 Paul Manias

#include "field_type_lookup.h"
#include <parasol/main.h>
#include <parasol/strings.hpp>
#include <span>
#include <cctype>
#include <algorithm>

// Map Parasol FD_* field flags to FluidType.
// Reference: lib_object.cpp shows the FD_* flag checking pattern.

static FluidType map_field_flags_to_fluid_type(uint32_t Flags)
{
   // NB: Order is important

   if (Flags & FD_ARRAY) return FluidType::Table; // TODO: Using FluidType::Array crashes - requires further investigation
   else if (Flags & FD_STRUCT) return FluidType::Table;
   else if (Flags & FD_STRING) return FluidType::Str;
   else if (Flags & (FD_OBJECT|FD_LOCAL)) return FluidType::Object;
   else if (Flags & FD_POINTER) return FluidType::Any; // Prefer runtime resolution for pointer types
   else if (Flags & (FD_DOUBLE|FD_FLOAT|FD_INT64|FD_INT)) return FluidType::Num;
   else if (Flags & FD_FUNCTION) return FluidType::Func;

   return FluidType::Any; // Prefer runtime resolution for unknown types
}

// If class not found, returns std::nullopt.
// If field not found, returns type FluidType::Unknown.

static std::optional<FieldTypeInfo> lookup_field_type(CLASSID ClassID, uint32_t FieldID)
{
   if (ClassID IS CLASSID::NIL) return std::nullopt;

   auto *meta_class = FindClass(ClassID);
   if (not meta_class) { // This should never happen - caller probably used an uninitialised variable
      pf::Log(__FUNCTION__).warning("Class ID $%.8x is invalid.", uint32_t(ClassID));
      return std::nullopt;
   }

   // TODO: Use FindField() for faster lookup
   Field *dict = nullptr;
   int total_dict = 0;
   if (meta_class->get(FID_Dictionary, dict, total_dict) IS ERR::Okay) {
      auto dict_span = std::span(dict, total_dict);
      for (const auto &field : dict_span) {
         if (field.FieldID IS FieldID) {
            FluidType type = map_field_flags_to_fluid_type(field.Flags);

            // For object fields, try to extract the class ID from the Arg field

            CLASSID object_class_id = CLASSID::NIL;
            if ((field.Flags & (FD_OBJECT|FD_LOCAL)) and (field.Arg)) {
               object_class_id = CLASSID(field.Arg);
            }

            return FieldTypeInfo { type, object_class_id };
         }
      }
   }

   // Field not found in dictionary - return Unknown type to signal error
   return FieldTypeInfo { FluidType::Unknown, CLASSID::NIL };
}
