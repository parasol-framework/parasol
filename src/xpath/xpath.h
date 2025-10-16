#pragma once

#include <parasol/system/errors.h>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <parasol/modules/xpath.h>

extern "C" ERR load_regex(void);

struct XPathNode {
   struct XPathAttributeValuePart
   {
      bool is_expression = false;
      std::string text;
   };

   struct XPathConstructorAttribute
   {
      std::string prefix;
      std::string name;
      std::string namespace_uri;
      bool is_namespace_declaration = false;
      std::vector<XPathAttributeValuePart> value_parts;
      std::vector<std::unique_ptr<XPathNode>> expression_parts;

      void set_expression_for_part(size_t index, std::unique_ptr<XPathNode> expr)
      {
         if (expression_parts.size() <= index) expression_parts.resize(index + 1);
         expression_parts[index] = std::move(expr);
      }

      [[nodiscard]] XPathNode * get_expression_for_part(size_t index) const
      {
         return index < expression_parts.size() ? expression_parts[index].get() : nullptr;
      }
   };

   struct XPathConstructorInfo
   {
      std::string prefix;
      std::string name;
      std::string namespace_uri;
      bool is_empty_element = false;
      bool is_direct = false;
      std::vector<XPathConstructorAttribute> attributes;
   };

   struct XPathOrderSpecOptions
   {
      bool is_descending = false;
      bool has_empty_mode = false;
      bool empty_is_greatest = false;
      std::string collation_uri;

      [[nodiscard]] bool has_collation() const
      {
         return !collation_uri.empty();
      }
   };

   struct XPathGroupKeyInfo
   {
      std::string variable_name;

      [[nodiscard]] bool has_variable() const
      {
         return !variable_name.empty();
      }
   };

   XPathNodeType type;
   std::string value;
   std::vector<std::unique_ptr<XPathNode>> children;
   std::optional<XPathConstructorInfo> constructor_info;
   std::vector<XPathAttributeValuePart> attribute_value_parts;
   bool attribute_value_has_expressions = false;
   std::unique_ptr<XPathNode> name_expression;
   bool order_clause_is_stable = false;
   std::optional<XPathOrderSpecOptions> order_spec_options;
   std::optional<XPathGroupKeyInfo> group_key_info;

   XPathNode(XPathNodeType t, std::string v = "") : type(t), value(std::move(v)) {}

   inline void add_child(std::unique_ptr<XPathNode> child) { children.push_back(std::move(child)); }
   [[nodiscard]] inline XPathNode * get_child(size_t index) const { return index < children.size() ? children[index].get() : nullptr; }
   [[nodiscard]] inline size_t child_count() const { return children.size(); }
   inline void set_constructor_info(XPathConstructorInfo info) { constructor_info = std::move(info); }
   [[nodiscard]] inline bool has_constructor_info() const { return constructor_info.has_value(); }
   inline void set_name_expression(std::unique_ptr<XPathNode> expr) { name_expression = std::move(expr); }
   [[nodiscard]] inline XPathNode * get_name_expression() const { return name_expression.get(); }
   [[nodiscard]] inline bool has_name_expression() const { return name_expression != nullptr; }
   inline void set_group_key_info(XPathGroupKeyInfo Info) { group_key_info = std::move(Info); }
   [[nodiscard]] inline bool has_group_key_info() const { return group_key_info.has_value(); }
   [[nodiscard]] inline const XPathGroupKeyInfo * get_group_key_info() const { return group_key_info ? &(*group_key_info) : nullptr; }

   void set_attribute_value_parts(std::vector<XPathAttributeValuePart> parts) {
      attribute_value_has_expressions = false;
      for (const auto &part : parts) {
         if (part.is_expression) {
            attribute_value_has_expressions = true;
            break;
         }
      }
      attribute_value_parts = std::move(parts);
   }


   inline void set_order_spec_options(XPathOrderSpecOptions Options) {
      order_spec_options = std::move(Options);
   }

   [[nodiscard]] inline bool has_order_spec_options() const {
      return order_spec_options.has_value();
   }

   [[nodiscard]] inline const XPathOrderSpecOptions * get_order_spec_options() const {
      return order_spec_options ? &(*order_spec_options) : nullptr;
   }
};
