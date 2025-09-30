#pragma once

#include <ankerl/unordered_dense.h>
#include <string>
#include <string_view>
#include <vector>

#include "schema_types.h"

namespace xml::schema
{
   struct SchemaDocument
   {
      std::string target_namespace;
      std::string schema_prefix;
      ankerl::unordered_dense::map<std::string, std::string> namespace_bindings;
      std::vector<std::shared_ptr<SchemaTypeDescriptor>> declared_types;

      void clear();
      [[nodiscard]] bool empty() const noexcept;
   };

   class SchemaParser
   {
      public:
      SchemaParser() = default;

      [[nodiscard]] SchemaDocument parse(const objXML::TAGS &Tags) const;
      [[nodiscard]] SchemaDocument parse(const XMLTag &Root) const;
   };
}
