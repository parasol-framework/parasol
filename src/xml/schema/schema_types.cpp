// schema_types.cpp - Implements schema type descriptors and registry utilities for the XML module.

#include "schema_types.h"
#include "../xml.h"
#include <utility>

namespace xml::schema
{
   namespace
   {
      // Creates a descriptor instance with the supplied metadata for registration.
      std::shared_ptr<SchemaTypeDescriptor> make_descriptor(SchemaType Type, std::string Name,
                                                            const std::shared_ptr<SchemaTypeDescriptor> &Base, bool Builtin)
      {
         return std::make_shared<SchemaTypeDescriptor>(Type, std::move(Name), Base, Builtin);
      }

      // Tests whether the provided schema type represents a string-like value.
      bool is_schema_string(SchemaType Type) noexcept
      {
         switch (Type) {
            case SchemaType::XPathString:
            case SchemaType::XSString:
               return true;
            default:
               return false;
         }
      }

      // Tests whether the provided schema type represents a numeric value category.
      bool is_schema_numeric(SchemaType Type) noexcept
      {
         switch (Type) {
            case SchemaType::XPathNumber:
            case SchemaType::XSDecimal:
            case SchemaType::XSFloat:
            case SchemaType::XSDouble:
            case SchemaType::XSInteger:
            case SchemaType::XSLong:
            case SchemaType::XSInt:
            case SchemaType::XSShort:
            case SchemaType::XSByte:
               return true;
            default:
               return false;
         }
      }
   }

   SchemaTypeDescriptor::SchemaTypeDescriptor(SchemaType Type, std::string Name,
                                              std::shared_ptr<SchemaTypeDescriptor> Base, bool Builtin)
      : base_type(Base), builtin_type(Builtin), schema_type(Type), type_name(std::move(Name))
   {
   }

   std::shared_ptr<SchemaTypeDescriptor> SchemaTypeDescriptor::base() const
   {
      return base_type.lock();
   }

   bool SchemaTypeDescriptor::is_builtin() const noexcept
   {
      return builtin_type;
   }

   // Determines whether the descriptor ultimately derives from the requested schema type.
   bool SchemaTypeDescriptor::is_derived_from(SchemaType Target) const
   {
      if (schema_type IS Target) return true;
      if (Target IS SchemaType::XSAnyType) return true;

      if (auto Parent = base_type.lock()) {
         return Parent->is_derived_from(Target);
      }

      return false;
   }

   // Reports whether the descriptor can legally coerce values into the requested type.
   bool SchemaTypeDescriptor::can_coerce_to(SchemaType Target) const
   {
      if (schema_type IS Target) return true;
      if (Target IS SchemaType::XSAnyType) return true;

      if (is_schema_numeric(schema_type) and is_schema_numeric(Target)) return true;
      if (is_schema_string(Target)) return true;

      if (auto Parent = base_type.lock()) {
         return Parent->can_coerce_to(Target);
      }

      return false;
   }

   // Converts an XPath value into the requested schema type when permitted.
   XPathValue SchemaTypeDescriptor::coerce_value(const XPathValue &Value, SchemaType Target) const
   {
      if ((schema_type IS Target) or (Target IS SchemaType::XSAnyType)) return Value;

      if ((Target IS SchemaType::XPathBoolean) or (Target IS SchemaType::XSBoolean)) {
         return XPathValue(Value.to_boolean());
      }

      if (is_schema_numeric(Target)) {
         return XPathValue(Value.to_number());
      }

      if (is_schema_string(Target)) {
         return XPathValue(Value.to_string());
      }

      return Value;
   }

   SchemaTypeRegistry::SchemaTypeRegistry()
   {
      register_builtin_types();
   }

   // Registers a descriptor for the given type if one does not already exist.
   std::shared_ptr<SchemaTypeDescriptor> SchemaTypeRegistry::register_descriptor(SchemaType Type, std::string Name,
                                                                                 std::shared_ptr<SchemaTypeDescriptor> Base,
                                                                                 bool Builtin)
   {
      auto Existing = find_descriptor(Type);
      if (Existing) return Existing;

      auto Descriptor = make_descriptor(Type, std::move(Name), Base, Builtin);
      descriptors_by_type.emplace(Type, Descriptor);
      descriptors_by_name.emplace(Descriptor->type_name, Descriptor);
      return Descriptor;
   }

   std::shared_ptr<SchemaTypeDescriptor> SchemaTypeRegistry::find_descriptor(SchemaType Type) const
   {
      auto Iter = descriptors_by_type.find(Type);
      if (Iter != descriptors_by_type.end()) return Iter->second;
      return nullptr;
   }

   std::shared_ptr<SchemaTypeDescriptor> SchemaTypeRegistry::find_descriptor(std::string_view Name) const
   {
      auto Iter = descriptors_by_name.find(std::string(Name));
      if (Iter != descriptors_by_name.end()) return Iter->second;
      return nullptr;
   }

   void SchemaTypeRegistry::clear()
   {
      descriptors_by_type.clear();
      descriptors_by_name.clear();
   }

   // Populates the registry with the built-in schema types recognised by Parasol.
   void SchemaTypeRegistry::register_builtin_types()
   {
      clear();

      auto AnyType = register_descriptor(SchemaType::XSAnyType, "xs:anyType", nullptr, true);

      register_descriptor(SchemaType::XPathNodeSet, "xpath:node-set", nullptr, true);
      register_descriptor(SchemaType::XPathBoolean, "xpath:boolean", nullptr, true);
      register_descriptor(SchemaType::XPathNumber, "xpath:number", nullptr, true);
      register_descriptor(SchemaType::XPathString, "xpath:string", nullptr, true);

      register_descriptor(SchemaType::XSString, "xs:string", AnyType, true);
      register_descriptor(SchemaType::XSBoolean, "xs:boolean", AnyType, true);
      auto DecimalType = register_descriptor(SchemaType::XSDecimal, "xs:decimal", AnyType, true);
      auto FloatType = register_descriptor(SchemaType::XSFloat, "xs:float", DecimalType, true);
      register_descriptor(SchemaType::XSDouble, "xs:double", FloatType, true);
      register_descriptor(SchemaType::XSDuration, "xs:duration", AnyType, true);
      register_descriptor(SchemaType::XSDateTime, "xs:dateTime", AnyType, true);
      register_descriptor(SchemaType::XSTime, "xs:time", AnyType, true);
      register_descriptor(SchemaType::XSDate, "xs:date", AnyType, true);
      auto IntegerType = register_descriptor(SchemaType::XSInteger, "xs:integer", DecimalType, true);
      auto LongType = register_descriptor(SchemaType::XSLong, "xs:long", IntegerType, true);
      auto IntType = register_descriptor(SchemaType::XSInt, "xs:int", LongType, true);
      auto ShortType = register_descriptor(SchemaType::XSShort, "xs:short", IntType, true);
      register_descriptor(SchemaType::XSByte, "xs:byte", ShortType, true);
   }

   SchemaTypeRegistry & registry()
   {
      static SchemaTypeRegistry global_registry;
      return global_registry;
   }

   bool is_numeric(SchemaType Type) noexcept
   {
      return is_schema_numeric(Type);
   }

   bool is_string_like(SchemaType Type) noexcept
   {
      return is_schema_string(Type);
   }

   // Maps an XPath runtime value type onto the corresponding schema type.
   SchemaType schema_type_for_xpath(XPathValueType Type) noexcept
   {
      switch (Type) {
         case XPathValueType::NodeSet:  return SchemaType::XPathNodeSet;
         case XPathValueType::Boolean:  return SchemaType::XPathBoolean;
         case XPathValueType::Number:   return SchemaType::XPathNumber;
         case XPathValueType::String:   return SchemaType::XPathString;
         case XPathValueType::Date:
         case XPathValueType::Time:
         case XPathValueType::DateTime: return SchemaType::XSDateTime;
      }

      return SchemaType::XPathString;
   }
}
