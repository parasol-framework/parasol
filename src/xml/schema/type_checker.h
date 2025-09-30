#pragma once

#include "schema_parser.h"

namespace xml::schema
{
   class TypeChecker
   {
      private:
      SchemaTypeRegistry * registry_ref;
      const SchemaContext * context_ref;

      public:
      explicit TypeChecker(SchemaTypeRegistry &Registry, const SchemaContext *Context = nullptr);

      void set_context(const SchemaContext *Context);
      [[nodiscard]] const SchemaContext * schema_context() const;

      [[nodiscard]] bool validate_value(const XPathValue &Value, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] bool validate_attribute(const XMLAttrib &Attribute, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] bool validate_node(const XMLTag &Tag, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] bool validate_element(const XMLTag &Tag, const ElementDescriptor &Descriptor) const;
      [[nodiscard]] SchemaTypeRegistry & registry() const;
   };
}
