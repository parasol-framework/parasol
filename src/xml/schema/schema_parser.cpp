#include "schema_parser.h"

#include <parasol/strings.hpp>

namespace xml::schema
{
   namespace
   {
      std::string_view extract_local_name(std::string_view Qualified) noexcept
      {
         auto colon = Qualified.find(':');
         if (colon != std::string::npos) return Qualified.substr(colon + 1u);
         return Qualified;
      }

      bool is_named(std::string_view Name, std::string_view Expected)
      {
         return pf::iequals(std::string(Name), std::string(Expected));
      }

      std::string make_qualified_name(std::string_view Prefix, std::string_view Local)
      {
         if (Prefix.empty()) return std::string(Local);

         std::string result(Prefix);
         result.push_back(':');
         result.append(Local);
         return result;
      }
   }

   void SchemaDocument::clear()
   {
      target_namespace.clear();
      schema_prefix.clear();
      namespace_bindings.clear();
      declared_types.clear();
   }

   bool SchemaDocument::empty() const noexcept
   {
      return declared_types.empty() and target_namespace.empty();
   }

   SchemaDocument SchemaParser::parse(const objXML::TAGS &Tags) const
   {
      SchemaDocument Document;
      if (Tags.empty()) return Document;
      return parse(Tags[0]);
   }

   SchemaDocument SchemaParser::parse(const XMLTag &Root) const
   {
      SchemaDocument Document;
      if (Root.Attribs.empty()) return Document;

      std::string_view root_name(Root.Attribs[0].Name);
      auto colon = root_name.find(':');
      if (colon != std::string::npos) Document.schema_prefix.assign(root_name.begin(), root_name.begin() + colon);
      else Document.schema_prefix.clear();

      for (size_t index = 1u; index < Root.Attribs.size(); ++index) {
         const auto &Attrib = Root.Attribs[index];
         if (pf::iequals(Attrib.Name, "targetNamespace")) {
            Document.target_namespace = Attrib.Value;
            continue;
         }

         std::string_view attrib_name(Attrib.Name);
         if (attrib_name.rfind("xmlns", 0) IS 0) {
            std::string prefix;
            if ((attrib_name.length() > 5u) and (attrib_name[5] IS ':')) {
               prefix.assign(attrib_name.begin() + 6u, attrib_name.end());
            }
            Document.namespace_bindings[prefix] = Attrib.Value;
         }
      }

      auto AnyType = registry().find_descriptor(SchemaType::XSAnyType);
      if (!AnyType) {
         AnyType = registry().register_descriptor(SchemaType::XSAnyType, "xs:anyType", nullptr, true);
      }

      for (const auto &Child : Root.Children) {
         if (Child.Attribs.empty()) continue;

         std::string_view child_name(Child.Attribs[0].Name);
         auto local_name = extract_local_name(child_name);
         if ((!is_named(local_name, "simpleType")) and (!is_named(local_name, "complexType"))) continue;

         std::string declared_name;
         for (size_t attr_index = 1u; attr_index < Child.Attribs.size(); ++attr_index) {
            const auto &ChildAttrib = Child.Attribs[attr_index];
            if (pf::iequals(ChildAttrib.Name, "name")) {
               declared_name = ChildAttrib.Value;
               break;
            }
         }

         if (declared_name.empty()) continue;

         auto Descriptor = std::make_shared<SchemaTypeDescriptor>(SchemaType::UserDefined,
                                                                  make_qualified_name(Document.schema_prefix, declared_name),
                                                                  AnyType, false);
         Document.declared_types.push_back(Descriptor);
      }

      return Document;
   }
}
