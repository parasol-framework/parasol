#include "schema_parser.h"

#include <cstdlib>
#include <parasol/strings.hpp>

namespace xml::schema
{
   namespace
   {
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

      std::string find_attribute_value(const XMLTag &Node, std::string_view Name)
      {
         for (size_t index = 1u; index < Node.Attribs.size(); ++index) {
            const auto &Attrib = Node.Attribs[index];
            if (pf::iequals(Attrib.Name, std::string(Name))) return Attrib.Value;
         }
         return std::string();
      }

      size_t parse_occurs_value(const std::string &Value, size_t DefaultValue, bool AllowUnbounded)
      {
         if (Value.empty()) return DefaultValue;
         if (AllowUnbounded and pf::iequals(Value, "unbounded")) return std::numeric_limits<size_t>::max();

         char *end = nullptr;
         auto parsed = std::strtoull(Value.c_str(), &end, 10);
         if ((end) and (*end IS '\0')) return static_cast<size_t>(parsed);
         return DefaultValue;
      }

      void register_element_aliases(SchemaDocument &Document, const std::shared_ptr<ElementDescriptor> &Descriptor)
      {
         if (!Descriptor) return;
         if (!Document.context) Document.context = std::make_shared<SchemaContext>();

         auto &elements = Document.context->elements;
         elements[Descriptor->name] = Descriptor;
         if (!Descriptor->qualified_name.empty()) elements[Descriptor->qualified_name] = Descriptor;

         auto local_name = std::string(extract_local_name(Descriptor->qualified_name.empty() ? Descriptor->name
                                                                                               : Descriptor->qualified_name));
         if (!local_name.empty()) elements[local_name] = Descriptor;

         if (!Document.target_namespace_prefix.empty()) {
            elements[make_qualified_name(Document.target_namespace_prefix, local_name)] = Descriptor;
         }

         for (const auto &entry : Document.namespace_bindings) {
            if (entry.second IS Document.target_namespace) {
               if (entry.first.empty()) elements[local_name] = Descriptor;
               else elements[make_qualified_name(entry.first, local_name)] = Descriptor;
            }
         }
      }
   }

   SchemaDocument::SchemaDocument()
      : context(std::make_shared<SchemaContext>())
   {
   }

   void SchemaDocument::merge_types()
   {
      if (!context) context = std::make_shared<SchemaContext>();

      for (const auto &Descriptor : declared_types) {
         if (!Descriptor) continue;

         auto &types = context->types;
         types[Descriptor->type_name] = Descriptor;

         auto local_name = std::string(extract_local_name(Descriptor->type_name));
         if (!local_name.empty()) types[local_name] = Descriptor;

         if (!target_namespace_prefix.empty()) {
            types[make_qualified_name(target_namespace_prefix, local_name)] = Descriptor;
         }

         for (const auto &entry : namespace_bindings) {
            if (entry.second IS target_namespace) {
               if (entry.first.empty()) types[local_name] = Descriptor;
               else types[make_qualified_name(entry.first, local_name)] = Descriptor;
            }
         }
      }
   }

   void SchemaDocument::clear()
   {
      if (context) {
         context->target_namespace.clear();
         context->schema_prefix.clear();
         context->target_namespace_prefix.clear();
         context->namespace_bindings.clear();
         context->types.clear();
         context->complex_types.clear();
         context->elements.clear();
      }

      target_namespace.clear();
      schema_prefix.clear();
      target_namespace_prefix.clear();
      namespace_bindings.clear();
      declared_types.clear();
   }

   bool SchemaDocument::empty() const noexcept
   {
      if (!context) return true;
      return declared_types.empty() and context->elements.empty() and context->complex_types.empty();
   }

   SchemaParser::SchemaParser(SchemaTypeRegistry &Registry)
      : registry_ref(&Registry)
   {
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
      if (!Document.context) Document.context = std::make_shared<SchemaContext>();

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
            if ((Document.target_namespace_prefix.empty()) and (Attrib.Value IS Document.target_namespace)) {
               Document.target_namespace_prefix = prefix;
            }
         }
      }

      if (Document.target_namespace_prefix.empty()) {
         for (const auto &entry : Document.namespace_bindings) {
            if (entry.second IS Document.target_namespace) {
               Document.target_namespace_prefix = entry.first;
               break;
            }
         }
      }

      if (Document.context) {
         Document.context->target_namespace = Document.target_namespace;
         Document.context->schema_prefix = Document.schema_prefix;
         Document.context->target_namespace_prefix = Document.target_namespace_prefix;
         Document.context->namespace_bindings = Document.namespace_bindings;
      }

      for (const auto &Child : Root.Children) {
         if (Child.Attribs.empty()) continue;

         std::string_view child_name(Child.Attribs[0].Name);
         auto local_name = extract_local_name(child_name);

         if (is_named(local_name, "simpleType")) {
            parse_simple_type(Child, Document);
            continue;
         }

         if (is_named(local_name, "complexType")) {
            parse_complex_type(Child, Document);
            continue;
         }

         if (is_named(local_name, "element")) {
            parse_element(Child, Document);
         }
      }

      Document.merge_types();
      return Document;
   }

   std::shared_ptr<SchemaContext> SchemaParser::parse_context(const XMLTag &Root) const
   {
      auto Document = parse(Root);
      return Document.context;
   }

   void SchemaParser::parse_simple_type(const XMLTag &Node, SchemaDocument &Document) const
   {
      auto declared_name = find_attribute_value(Node, "name");
      if (declared_name.empty()) return;

      std::string base_name;
      for (const auto &Child : Node.Children) {
         if (Child.Attribs.empty()) continue;
         auto child_local = extract_local_name(Child.Attribs[0].Name);
         if (!is_named(child_local, "restriction")) continue;

         base_name = find_attribute_value(Child, "base");
         break;
      }

      auto BaseDescriptor = resolve_type(base_name, Document);
      if (!BaseDescriptor) BaseDescriptor = registry_ref->find_descriptor(SchemaType::XSAnyType);

      auto qualified_name = Document.target_namespace_prefix.empty()
         ? declared_name
         : make_qualified_name(Document.target_namespace_prefix, declared_name);

      auto Descriptor = std::make_shared<SchemaTypeDescriptor>(SchemaType::UserDefined, qualified_name, BaseDescriptor, false);
      Document.declared_types.push_back(Descriptor);

      auto &types = Document.context->types;
      types[qualified_name] = Descriptor;
      types[declared_name] = Descriptor;
   }

   void SchemaParser::parse_complex_type(const XMLTag &Node, SchemaDocument &Document) const
   {
      auto declared_name = find_attribute_value(Node, "name");
      if (declared_name.empty()) return;

      auto Descriptor = std::make_shared<ElementDescriptor>();
      Descriptor->name = declared_name;
      Descriptor->qualified_name = Document.target_namespace_prefix.empty()
         ? declared_name
         : make_qualified_name(Document.target_namespace_prefix, declared_name);

      for (const auto &Child : Node.Children) {
         if (Child.Attribs.empty()) continue;

         auto child_local = extract_local_name(Child.Attribs[0].Name);
         if (!is_named(child_local, "sequence")) continue;

         for (const auto &SequenceChild : Child.Children) {
            if (SequenceChild.Attribs.empty()) continue;
            auto seq_local = extract_local_name(SequenceChild.Attribs[0].Name);
            if (!is_named(seq_local, "element")) continue;

            auto element_name = find_attribute_value(SequenceChild, "name");
            if (element_name.empty()) continue;

            auto element_descriptor = std::make_shared<ElementDescriptor>();
            element_descriptor->name = element_name;
            element_descriptor->qualified_name = Document.target_namespace_prefix.empty()
               ? element_name
               : make_qualified_name(Document.target_namespace_prefix, element_name);

            auto type_name = find_attribute_value(SequenceChild, "type");
            element_descriptor->type_name = type_name;
            if (!type_name.empty()) element_descriptor->type = resolve_type(type_name, Document);

            element_descriptor->min_occurs = parse_occurs_value(find_attribute_value(SequenceChild, "minOccurs"), 1u, false);
            element_descriptor->max_occurs = parse_occurs_value(find_attribute_value(SequenceChild, "maxOccurs"), 1u, true);

            Descriptor->children.push_back(element_descriptor);
         }
      }

      Document.context->complex_types[Descriptor->name] = Descriptor;
      Document.context->complex_types[Descriptor->qualified_name] = Descriptor;
   }

   void SchemaParser::parse_element(const XMLTag &Node, SchemaDocument &Document) const
   {
      auto declared_name = find_attribute_value(Node, "name");
      if (declared_name.empty()) return;

      auto Descriptor = std::make_shared<ElementDescriptor>();
      Descriptor->name = declared_name;
      Descriptor->qualified_name = Document.target_namespace_prefix.empty()
         ? declared_name
         : make_qualified_name(Document.target_namespace_prefix, declared_name);

      auto type_name = find_attribute_value(Node, "type");
      Descriptor->type_name = type_name;

      if (!type_name.empty()) {
         Descriptor->type = resolve_type(type_name, Document);

         auto complex_it = Document.context->complex_types.find(type_name);
         if (complex_it != Document.context->complex_types.end()) Descriptor->children = complex_it->second->children;
         else {
            auto local_name = std::string(extract_local_name(type_name));
            complex_it = Document.context->complex_types.find(local_name);
            if (complex_it != Document.context->complex_types.end()) Descriptor->children = complex_it->second->children;
         }
      }

      register_element_aliases(Document, Descriptor);
   }

   std::shared_ptr<SchemaTypeDescriptor> SchemaParser::resolve_type(std::string_view Name, SchemaDocument &Document) const
   {
      if (!registry_ref) registry_ref = &registry();
      if (Name.empty()) return registry_ref->find_descriptor(SchemaType::XSAnyType);

      auto lookup_name = std::string(Name);
      auto &types = Document.context->types;
      auto iter = types.find(lookup_name);
      if (iter != types.end()) return iter->second;

      auto descriptor = registry_ref->find_descriptor(Name);
      if (descriptor) return descriptor;

      auto local_name = std::string(extract_local_name(Name));
      iter = types.find(local_name);
      if (iter != types.end()) return iter->second;

      descriptor = registry_ref->find_descriptor(local_name);
      if (descriptor) return descriptor;

      return registry_ref->find_descriptor(SchemaType::XSAnyType);
   }

   std::string_view extract_local_name(std::string_view Qualified) noexcept
   {
      auto colon = Qualified.find(':');
      if (colon != std::string::npos) return Qualified.substr(colon + 1u);
      return Qualified;
   }
}
