#pragma once

#include "schema_types.h"

namespace xml::schema
{
   class TypeChecker
   {
      private:
      SchemaTypeRegistry * registry_ref;

      public:
      explicit TypeChecker(SchemaTypeRegistry &Registry);

      [[nodiscard]] bool validate_value(const XPathValue &Value, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] bool validate_attribute(const XMLAttrib &Attribute, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] bool validate_node(const XMLTag &Tag, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] SchemaTypeRegistry & registry() const;
   };
}
