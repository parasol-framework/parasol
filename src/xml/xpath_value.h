#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/xpath.h>

namespace xml::schema
{
   enum class SchemaType;
   class SchemaTypeDescriptor;
   class SchemaTypeRegistry;
   [[nodiscard]] SchemaType schema_type_for_xpath(XPVT) noexcept;
   [[nodiscard]] bool is_numeric(SchemaType) noexcept;
   SchemaTypeRegistry & registry();
}

class XPathVal : public XPathValue
{
   public:
   XPVT type;
   std::vector<XMLTag *> node_set;
   std::optional<std::string> node_set_string_override;
   std::vector<std::string> node_set_string_values;
   std::vector<const XMLAttrib *> node_set_attributes;
   bool boolean_value = false;
   double number_value = 0.0;
   std::string string_value;
   mutable std::shared_ptr<xml::schema::SchemaTypeDescriptor> schema_type_info;
   mutable bool schema_validated = false;

   // Constructors

   XPathVal() : type(XPVT::Boolean) {}
   explicit XPathVal(bool value) : type(XPVT::Boolean), boolean_value(value) {}
   explicit XPathVal(double value) : type(XPVT::Number), number_value(value) {}
   explicit XPathVal(std::string value) : type(XPVT::String), string_value(std::move(value)) {}
   explicit XPathVal(XPVT ValueType, std::string value) : type(ValueType), string_value(std::move(value)) {}
   explicit XPathVal(const std::vector<XMLTag *> &Nodes,
                       std::optional<std::string> NodeSetString = std::nullopt,
                       std::vector<std::string> NodeSetStrings = {},
                       std::vector<const XMLAttrib *> NodeSetAttributes = {})
      : type(XPVT::NodeSet),
        node_set(Nodes),
        node_set_string_override(std::move(NodeSetString)),
        node_set_string_values(std::move(NodeSetStrings)),
        node_set_attributes(std::move(NodeSetAttributes)) {}

   // Methods

   bool to_boolean() const;
   double to_number() const;
   std::string to_string() const;
   std::vector<XMLTag *> to_node_set() const;

   bool is_empty() const;
   size_t size() const;

   bool has_schema_info() const;
   void set_schema_type(std::shared_ptr<xml::schema::SchemaTypeDescriptor> TypeInfo);
   bool validate_against_schema() const;
   xml::schema::SchemaType get_schema_type() const;

   static std::string node_string_value(XMLTag *Node);
   static double string_to_number(const std::string &Value);
};

std::string format_xpath_number(double Value);
std::optional<bool> parse_schema_boolean(std::string_view Value);

