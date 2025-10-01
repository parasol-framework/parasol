
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

   TypeChecker::TypeChecker(SchemaTypeRegistry &Registry, const SchemaContext *Context, std::string *ErrorSink)
      : registry_ref(&Registry), context_ref(Context), error_sink(ErrorSink)
   {
   }

   void TypeChecker::set_context(const SchemaContext *Context)
   {
      context_ref = Context;
   }

   void TypeChecker::set_error_sink(std::string *ErrorSink)
   {
      error_sink = ErrorSink;
      if (error_sink and not last_error_message.empty()) *error_sink = last_error_message;
   }

   void TypeChecker::clear_error() const
   {
      last_error_message.clear();
      if (error_sink) error_sink->clear();
   }

   const SchemaContext * TypeChecker::schema_context() const
   {
      return context_ref;
   }

   const std::string & TypeChecker::last_error() const
   {
      return last_error_message;
   }

   void TypeChecker::assign_error(std::string Message) const
   {
      last_error_message.assign(std::move(Message));
      if (error_sink) *error_sink = last_error_message;
   }

   bool TypeChecker::validate_value(const XPathValue &Value, const SchemaTypeDescriptor &Descriptor) const
   {
      auto effective = resolve_effective_descriptor(Descriptor);
      auto target_type = effective->schema_type;

      if (is_numeric(target_type)) {
         auto coerced = effective->coerce_value(Value, target_type);
         if (!std::isnan(coerced.to_number())) return true;

         auto message = std::string("Value '");
         message.append(Value.to_string());
         message.append("' is not valid for type ");
         message.append(effective->type_name);
         message.push_back('.');
         assign_error(std::move(message));
         return false;
      }

      if ((target_type IS SchemaType::XPathBoolean) or (target_type IS SchemaType::XSBoolean)) {
         if (Value.type IS XPathValueType::Boolean) return true;
         auto string_value = Value.to_string();
         if (is_valid_boolean(string_value)) return true;

         auto message = std::string("Value '");
         message.append(string_value);
         message.append("' is not a recognised boolean value.");
         assign_error(std::move(message));
         return false;
      }

      if ((target_type IS SchemaType::XPathString) or (target_type IS SchemaType::XSString)) return true;
      if (target_type IS SchemaType::XPathNodeSet) {
         if (Value.type IS XPathValueType::NodeSet) return true;
         assign_error("Expected a node-set value.");
         return false;
      }

      auto SourceType = schema_type_for_xpath(Value.type);
      auto SourceDescriptor = registry().find_descriptor(SourceType);
      if (not SourceDescriptor) {
         assign_error("Unsupported value type for schema coercion.");
         return false;
      }

      if (SourceDescriptor->can_coerce_to(Descriptor.schema_type)) return true;

      auto message = std::string("Cannot coerce value of type ");
      message.append(SourceDescriptor->type_name);
      message.append(" to required type ");
      message.append(Descriptor.type_name);
      message.push_back('.');
      assign_error(std::move(message));
      return false;
   }

   bool TypeChecker::validate_attribute(const XMLAttrib &Attribute, const SchemaTypeDescriptor &Descriptor) const
   {
      XPathValue value(Attribute.Value);
      if (validate_value(value, Descriptor)) return true;

      std::string previous_error(last_error_message);
      auto message = std::string("Attribute ");
      if (!Attribute.Name.empty()) message.append(Attribute.Name);
      else message.append("(unnamed)");
      message.append(": ");

      if (!previous_error.empty()) message.append(previous_error);
      else {
         message.append("Value does not match expected type ");
         message.append(Descriptor.type_name);
      }

      assign_error(std::move(message));
      return false;
   }

   bool TypeChecker::validate_node(const XMLTag &Tag, const SchemaTypeDescriptor &Descriptor) const
   {
      if (Descriptor.schema_type IS SchemaType::XPathNodeSet) return true;
      if (Descriptor.can_coerce_to(SchemaType::XPathString)) {
         if (Tag.hasContent()) return true;
         if (not Tag.Children.empty()) return true;

         auto message = std::string("Element ");
         if (!Tag.Attribs.empty() and !Tag.Attribs[0].Name.empty()) message.append(Tag.Attribs[0].Name);
         else message.append("(unnamed)");
         message.append(" is missing required textual content.");
         assign_error(std::move(message));
         return false;
      }
      if (Descriptor.can_coerce_to(SchemaType::XPathNodeSet)) return true;

      assign_error("Element does not satisfy required node constraints.");
      return false;
   }

   bool TypeChecker::validate_element(const XMLTag &Tag, const ElementDescriptor &Descriptor) const
   {
      if (Descriptor.type and Descriptor.children.empty()) {
         XPathValue value(Tag.getContent());
         if (validate_value(value, *Descriptor.type)) return true;

         std::string previous_error(last_error_message);
         auto message = std::string("Element ");
         if (!Tag.Attribs.empty() and !Tag.Attribs[0].Name.empty()) message.append(Tag.Attribs[0].Name);
         else message.append("(unnamed)");
         message.append(": ");

         if (!previous_error.empty()) message.append(previous_error);
         else {
            message.append("Content does not match expected type ");
            message.append(Descriptor.type_name);
         }

         assign_error(std::move(message));
         return false;
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

         if (!rule) {
            std::string message("Element ");
            if (!Tag.Attribs.empty() and !Tag.Attribs[0].Name.empty()) message.append(Tag.Attribs[0].Name);
            else message.append("(unnamed)");
            message += " contains unexpected child element " + std::string(child_name);
            assign_error(std::move(message));
            return false;
         }

         counters[rule]++;

         if (rule->type and rule->children.empty()) {
            XPathValue child_value(Child.getContent());
            if (!validate_value(child_value, *rule->type)) {
               std::string previous_error(last_error_message);
               std::string message("Element ");
               if (!Child.Attribs.empty() and !Child.Attribs[0].Name.empty()) message.append(Child.Attribs[0].Name);
               else message.append("(unnamed)");
               message.append(": ");

               if (!previous_error.empty()) message.append(previous_error);
               else {
                  message.append("Content does not match expected type ");
                  message.append(rule->type->type_name);
               }

               assign_error(std::move(message));
               return false;
            }
         }
         else if (!rule->children.empty()) {
            if (!validate_element(Child, *rule)) return false;
         }
      }

      for (const auto &child : Descriptor.children) {
         if (!child) continue;
         size_t count = counters[child.get()];
         if (count < child->min_occurs) {
            std::string message("Element ");
            if (!Tag.Attribs.empty() and !Tag.Attribs[0].Name.empty()) message.append(Tag.Attribs[0].Name);
            else message.append("(unnamed)");
            message += " is missing required child element " + child->name + " (expected at least " + std::to_string(child->min_occurs) + ").";
            assign_error(std::move(message));
            return false;
         }
         if ((child->max_occurs != std::numeric_limits<size_t>::max()) and (count > child->max_occurs)) {
            std::string message("Element ");
            if (!Tag.Attribs.empty() and !Tag.Attribs[0].Name.empty()) message.append(Tag.Attribs[0].Name);
            else message.append("(unnamed)");
            message += " contains too many " + child->name + " elements (maximum allowed is " + std::to_string(child->max_occurs) + ").";
            assign_error(std::move(message));
            return false;
         }
      }

      return true;
   }

   SchemaTypeRegistry & TypeChecker::registry() const
   {
      return *registry_ref;
   }
}
