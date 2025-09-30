#include "type_checker.h"

namespace xml::schema
{
   TypeChecker::TypeChecker(SchemaTypeRegistry &Registry)
      : registry_ref(&Registry)
   {
   }

   bool TypeChecker::validate_value(const XPathValue &Value, const SchemaTypeDescriptor &Descriptor) const
   {
      auto SourceType = schema_type_for_xpath(Value.type);
      auto SourceDescriptor = registry().find_descriptor(SourceType);
      if (not SourceDescriptor) return false;
      return SourceDescriptor->can_coerce_to(Descriptor.schema_type);
   }

   bool TypeChecker::validate_attribute(const XMLAttrib &Attribute, const SchemaTypeDescriptor &Descriptor) const
   {
      (void)Attribute;
      return Descriptor.can_coerce_to(SchemaType::XPathString) or (Descriptor.schema_type IS SchemaType::UserDefined);
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

   SchemaTypeRegistry & TypeChecker::registry() const
   {
      return *registry_ref;
   }
}
