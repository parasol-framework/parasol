// schema_types.h - Declares schema type descriptors and registry helpers for XML validation.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <ankerl/unordered_dense.h>
#include <parasol/modules/xml.h>
#include "../xpath/xpath_functions.h"

namespace xml::schema
{
   enum class SchemaType
   {
      XPathNodeSet,
      XPathBoolean,
      XPathNumber,
      XPathString,
      XSAnyType,
      XSString,
      XSBoolean,
      XSDecimal,
      XSFloat,
      XSDouble,
      XSDuration,
      XSDateTime,
      XSTime,
      XSDate,
      XSInteger,
      XSLong,
      XSInt,
      XSShort,
      XSByte,
      UserDefined
   };

   class SchemaTypeDescriptor : public std::enable_shared_from_this<SchemaTypeDescriptor>
   {
      private:
      std::weak_ptr<SchemaTypeDescriptor> base_type;
      bool builtin_type;

      public:
      SchemaType schema_type;
      std::string type_name;

      SchemaTypeDescriptor(SchemaType Type, std::string Name, std::shared_ptr<SchemaTypeDescriptor> Base = nullptr,
                           bool Builtin = false);

      [[nodiscard]] std::shared_ptr<SchemaTypeDescriptor> base() const;
      [[nodiscard]] bool is_builtin() const noexcept;
      [[nodiscard]] bool is_derived_from(SchemaType Target) const;
      [[nodiscard]] bool can_coerce_to(SchemaType Target) const;
      [[nodiscard]] XPathValue coerce_value(const XPathValue &Value, SchemaType Target) const;
   };

   class SchemaTypeRegistry
   {
      private:
      ankerl::unordered_dense::map<SchemaType, std::shared_ptr<SchemaTypeDescriptor>> descriptors_by_type;
      ankerl::unordered_dense::map<std::string, std::shared_ptr<SchemaTypeDescriptor>> descriptors_by_name;

      void register_builtin_types();

      public:
      SchemaTypeRegistry();

      [[nodiscard]] std::shared_ptr<SchemaTypeDescriptor> register_descriptor(SchemaType Type, std::string Name,
                                                                             std::shared_ptr<SchemaTypeDescriptor> Base = nullptr,
                                                                             bool Builtin = false);
      [[nodiscard]] std::shared_ptr<SchemaTypeDescriptor> find_descriptor(SchemaType Type) const;
      [[nodiscard]] std::shared_ptr<SchemaTypeDescriptor> find_descriptor(std::string_view Name) const;
      void clear();
   };

   SchemaTypeRegistry & registry();

   [[nodiscard]] bool is_numeric(SchemaType Type) noexcept;
   [[nodiscard]] bool is_string_like(SchemaType Type) noexcept;
   [[nodiscard]] SchemaType schema_type_for_xpath(XPathValueType Type) noexcept;
}
