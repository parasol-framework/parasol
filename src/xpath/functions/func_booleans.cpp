//********************************************************************************************************************
// XPath Boolean Functions

XPathValue XPathFunctionLibrary::function_boolean(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(false);
   return XPathValue(Args[0].to_boolean());
}

XPathValue XPathFunctionLibrary::function_not(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(true);
   return XPathValue(!Args[0].to_boolean());
}

XPathValue XPathFunctionLibrary::function_true(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   return XPathValue(true);
}

XPathValue XPathFunctionLibrary::function_false(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   return XPathValue(false);
}

XPathValue XPathFunctionLibrary::function_lang(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(false);

   std::string requested = Args[0].to_string();
   if (requested.empty()) return XPathValue(false);

   XMLTag *node = Context.context_node;
   if (not node) return XPathValue(false);

   std::string language = find_language_for_node(node, Context.document);
   if (language.empty()) return XPathValue(false);

   return XPathValue(language_matches(language, requested));
}

XPathValue XPathFunctionLibrary::function_exists(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(false);

   const XPathValue &value = Args[0];

   if (value.type IS XPVT::NodeSet) {
      if (not value.node_set.empty()) return XPathValue(true);

      if (value.node_set_string_override.has_value()) return XPathValue(true);

      if (not value.node_set_string_values.empty()) return XPathValue(true);

      if (not value.node_set_attributes.empty()) return XPathValue(true);

      return XPathValue(false);
   }

   return XPathValue(true);
}

