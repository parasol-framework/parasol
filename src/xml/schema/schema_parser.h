#pragma once

#include <ankerl/unordered_dense.h>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "schema_types.h"

namespace xml::schema
{
   struct ElementDescriptor
   {
      std::string name;
      std::string qualified_name;
      std::string type_name;
      std::shared_ptr<SchemaTypeDescriptor> type;
      size_t min_occurs = 1u;
      size_t max_occurs = std::numeric_limits<size_t>::max();
      std::vector<std::shared_ptr<ElementDescriptor>> children;
   };

   struct SchemaContext
   {
      std::string target_namespace;
      std::string schema_prefix;
      std::string target_namespace_prefix;
      ankerl::unordered_dense::map<std::string, std::string> namespace_bindings;
      ankerl::unordered_dense::map<std::string, std::shared_ptr<SchemaTypeDescriptor>> types;
      ankerl::unordered_dense::map<std::string, std::shared_ptr<ElementDescriptor>> complex_types;
      ankerl::unordered_dense::map<std::string, std::shared_ptr<ElementDescriptor>> elements;
   };

   struct SchemaDocument
   {
      SchemaDocument();

      std::shared_ptr<SchemaContext> context;
      std::string target_namespace;
      std::string schema_prefix;
      std::string target_namespace_prefix;
      ankerl::unordered_dense::map<std::string, std::string> namespace_bindings;
      std::vector<std::shared_ptr<SchemaTypeDescriptor>> declared_types;

      void merge_types();

      void clear();
      [[nodiscard]] bool empty() const noexcept;
   };

   class SchemaParser
   {
      public:
      explicit SchemaParser(SchemaTypeRegistry &Registry);

      [[nodiscard]] SchemaDocument parse(const objXML::TAGS &Tags) const;
      [[nodiscard]] SchemaDocument parse(const XMLTag &Root) const;
      [[nodiscard]] std::shared_ptr<SchemaContext> parse_context(const XMLTag &Root) const;

      private:
      SchemaTypeRegistry * registry_ref;

      void parse_simple_type(const XMLTag &Node, SchemaDocument &Document) const;
      void parse_complex_type(const XMLTag &Node, SchemaDocument &Document) const;
      void parse_element(const XMLTag &Node, SchemaDocument &Document) const;
      void parse_inline_complex_type(const XMLTag &Node, SchemaDocument &Document, ElementDescriptor &Descriptor) const;
      void parse_sequence(const XMLTag &Node, SchemaDocument &Document, ElementDescriptor &Descriptor) const;
      [[nodiscard]] std::shared_ptr<ElementDescriptor> parse_child_element_descriptor(const XMLTag &Node,
                                                                                      SchemaDocument &Document) const;
      [[nodiscard]] std::shared_ptr<SchemaTypeDescriptor> resolve_type(std::string_view Name,
                                                                       SchemaDocument &Document) const;
   };

   [[nodiscard]] std::string_view extract_local_name(std::string_view Qualified) noexcept;
}
