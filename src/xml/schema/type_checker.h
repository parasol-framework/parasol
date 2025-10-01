// type_checker.h - Declares XML schema validation utilities for checking instance documents.

#pragma once

#include "schema_parser.h"
#include <string>

namespace xml::schema
{
   class TypeChecker
   {
      private:
      SchemaTypeRegistry * registry_ref;
      const SchemaContext * context_ref;
      std::string * error_sink;
      mutable std::string last_error_message;

      void assign_error(std::string Message) const;

      public:
      explicit TypeChecker(SchemaTypeRegistry &Registry, const SchemaContext *Context = nullptr,
                           std::string *ErrorSink = nullptr);

      void set_context(const SchemaContext *Context);
      void set_error_sink(std::string *ErrorSink);
      void clear_error() const;
      [[nodiscard]] const SchemaContext * schema_context() const;
      [[nodiscard]] const std::string & last_error() const;

      [[nodiscard]] bool validate_value(const XPathValue &Value, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] bool validate_attribute(const XMLAttrib &Attribute, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] bool validate_node(const XMLTag &Tag, const SchemaTypeDescriptor &Descriptor) const;
      [[nodiscard]] bool validate_element(const XMLTag &Tag, const ElementDescriptor &Descriptor) const;
      [[nodiscard]] SchemaTypeRegistry & registry() const;
   };
}
