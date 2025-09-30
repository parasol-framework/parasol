#include "type_checker.h"

#include <cmath>
#include <limits>

namespace xml::schema
{
   namespace
   {
      const SchemaTypeDescriptor * resolve_effective_descriptor(const SchemaTypeDescriptor &Descriptor)
      {
         const SchemaTypeDescriptor *current = &Descriptor;
         while (current and (current->schema_type IS SchemaType::UserDefined)) {
            auto base = current->base();
            if (!base) break;
            current = base.get();
         }
         return current ? current : &Descriptor;
      }

      bool is_valid_boolean(std::string_view Value)
      {
         if (pf::iequals(Value, "true")) return true;
         if (pf::iequals(Value, "false")) return true;
         if (Value.length() IS 1) {
            char ch = Value[0];
            return (ch IS '0') or (ch IS '1');
         }
         return false;
      }
   }

   TypeChecker::TypeChecker(SchemaTypeRegistry &Registry, const SchemaContext *Context)
      : registry_ref(&Registry), context_ref(Context)
   {
   }

   void TypeChecker::set_context(const SchemaContext *Context)
   {
      context_ref = Context;
   }

   const SchemaContext * TypeChecker::schema_context() const
   {
      return context_ref;
   }

   bool TypeChecker::validate_value(const XPathValue &Value, const SchemaTypeDescriptor &Descriptor) const
   {
      auto effective = resolve_effective_descriptor(Descriptor);
      auto target_type = effective->schema_type;

      if (is_numeric(target_type)) {
         auto coerced = effective->coerce_value(Value, target_type);
         return !std::isnan(coerced.to_number());
      }

      if ((target_type IS SchemaType::XPathBoolean) or (target_type IS SchemaType::XSBoolean)) {
         if (Value.type IS XPathValueType::Boolean) return true;
         auto string_value = Value.to_string();
         return is_valid_boolean(string_value);
      }

      if ((target_type IS SchemaType::XPathString) or (target_type IS SchemaType::XSString)) return true;
      if (target_type IS SchemaType::XPathNodeSet) return Value.type IS XPathValueType::NodeSet;

      auto SourceType = schema_type_for_xpath(Value.type);
      auto SourceDescriptor = registry().find_descriptor(SourceType);
      if (not SourceDescriptor) return false;
      return SourceDescriptor->can_coerce_to(Descriptor.schema_type);
   }

   bool TypeChecker::validate_attribute(const XMLAttrib &Attribute, const SchemaTypeDescriptor &Descriptor) const
   {
      XPathValue value(Attribute.Value);
      return validate_value(value, Descriptor);
   }

   bool TypeChecker::validate_node(const XMLTag &Tag, const SchemaTypeDescriptor &Descriptor) const
   {
      if (Descriptor.schema_type IS SchemaType::XPathNodeSet) return true;
      if (Descriptor.can_coerce_to(SchemaType::XPathString)) {
         if (Tag.hasContent()) return true;
         return not Tag.Children.empty();
      }
      return Descriptor.can_coerce_to(SchemaType::XPathNodeSet);
   }

   bool TypeChecker::validate_element(const XMLTag &Tag, const ElementDescriptor &Descriptor) const
   {
      if (Descriptor.type and Descriptor.children.empty()) {
         XPathValue value(Tag.getContent());
         return validate_value(value, *Descriptor.type);
      }

      if (Descriptor.children.empty()) return true;

      ankerl::unordered_dense::map<const ElementDescriptor *, size_t> counters;
      ankerl::unordered_dense::map<std::string, const ElementDescriptor *> lookup;

      for (const auto &child : Descriptor.children) {
         if (!child) continue;
         lookup[child->name] = child.get();
         if (!child->qualified_name.empty()) lookup[child->qualified_name] = child.get();

         auto local_name = std::string(extract_local_name(child->qualified_name.empty() ? child->name
                                                                                        : child->qualified_name));
         if (!local_name.empty()) lookup[local_name] = child.get();
         counters[child.get()] = 0u;
      }

      for (const auto &Child : Tag.Children) {
         if (Child.Attribs.empty()) continue;
         if (Child.Attribs[0].isContent()) continue;

         std::string_view child_name(Child.Attribs[0].Name);
         const ElementDescriptor *rule = nullptr;

         auto iter = lookup.find(std::string(child_name));
         if (iter != lookup.end()) rule = iter->second;
         else {
            auto local_name = std::string(extract_local_name(child_name));
            auto local_iter = lookup.find(local_name);
            if (local_iter != lookup.end()) rule = local_iter->second;
         }

         if (!rule) continue;

         counters[rule]++;

         if (rule->type and rule->children.empty()) {
            XPathValue child_value(Child.getContent());
            if (!validate_value(child_value, *rule->type)) return false;
         }
         else if (!rule->children.empty()) {
            if (!validate_element(Child, *rule)) return false;
         }
      }

      for (const auto &child : Descriptor.children) {
         if (!child) continue;
         size_t count = counters[child.get()];
         if (count < child->min_occurs) return false;
         if ((child->max_occurs != std::numeric_limits<size_t>::max()) and (count > child->max_occurs)) return false;
      }

      return true;
   }

   SchemaTypeRegistry & TypeChecker::registry() const
   {
      return *registry_ref;
   }
}
