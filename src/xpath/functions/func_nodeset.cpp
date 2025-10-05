//********************************************************************************************************************
// XPath Node Set Functions

XPathValue XPathFunctionLibrary::function_last(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue((double)Context.size);
}

XPathValue XPathFunctionLibrary::function_position(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue((double)Context.position);
}

XPathValue XPathFunctionLibrary::function_count(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(0.0);
   if (Args[0].type != XPVT::NodeSet) return XPathValue(0.0);
   return XPathValue((double)Args[0].node_set.size());
}

XPathValue XPathFunctionLibrary::function_id(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   std::vector<XMLTag *> results;

   if (Args.empty()) return XPathValue(results);

   std::unordered_set<std::string> requested_ids;

   auto add_tokens = [&](const std::string &Value) {
      size_t start = Value.find_first_not_of(" \t\r\n");
      while (start != std::string::npos) {
         size_t end = Value.find_first_of(" \t\r\n", start);
         std::string token = Value.substr(start, (end IS std::string::npos) ? std::string::npos : end - start);
         if (not token.empty()) requested_ids.insert(token);
         if (end IS std::string::npos) break;
         start = Value.find_first_not_of(" \t\r\n", end);
      }
   };

   auto collect_from_value = [&](const XPathValue &Value) {
      switch (Value.type) {
         case XPVT::NodeSet: {
            if (not Value.node_set_string_values.empty()) {
               for (const auto &entry : Value.node_set_string_values) add_tokens(entry);
            }
            else if (Value.node_set_string_override.has_value()) add_tokens(*Value.node_set_string_override);
            else {
               for (auto *node : Value.node_set) {
                  if (not node) continue;
                  add_tokens(node->getContent());
               }
            }
            break;
         }

         case XPVT::String:
         case XPVT::Date:
         case XPVT::Time:
         case XPVT::DateTime:
            add_tokens(Value.string_value);
            break;

         case XPVT::Boolean:
            add_tokens(Value.to_string());
            break;

         case XPVT::Number:
            if (not std::isnan(Value.number_value)) add_tokens(Value.to_string());
            break;
      }
   };

   for (const auto &arg : Args) collect_from_value(arg);

   if (requested_ids.empty()) return XPathValue(results);
   if (not Context.document) return XPathValue(results);

   std::unordered_set<int> seen_tags;

   std::function<void(XMLTag &)> visit = [&](XMLTag &Tag) {
      if (Tag.isTag()) {
         for (size_t index = 1; index < Tag.Attribs.size(); ++index) {
            const auto &attrib = Tag.Attribs[index];
            if (not (pf::iequals(attrib.Name, "id") or pf::iequals(attrib.Name, "xml:id"))) continue;

            size_t start = attrib.Value.find_first_not_of(" \t\r\n");
            while (start != std::string::npos) {
               size_t end = attrib.Value.find_first_of(" \t\r\n", start);
               std::string token = attrib.Value.substr(start, (end IS std::string::npos) ? std::string::npos : end - start);
               if (not token.empty() and (requested_ids.find(token) != requested_ids.end())) {
                  if (seen_tags.insert(Tag.ID).second) results.push_back(&Tag);
                  break;
               }
               if (end IS std::string::npos) break;
               start = attrib.Value.find_first_not_of(" \t\r\n", end);
            }
         }
      }

      for (auto &child : Tag.Children) visit(child);
   };

   for (auto &root : Context.document->Tags) visit(root);

   return XPathValue(results);
}

XPathValue XPathFunctionLibrary::function_local_name(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPVT::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue(std::string());

   if (target_attribute) {
      std::string_view name = target_attribute->Name;
      auto colon = name.find(':');
      if (colon IS std::string::npos) return XPathValue(std::string(name));
      return XPathValue(std::string(name.substr(colon + 1)));
   }

   if (not target_node) return XPathValue(std::string());
   if (target_node->Attribs.empty()) return XPathValue(std::string());

   std::string_view node_name = target_node->Attribs[0].Name;
   if (node_name.empty()) return XPathValue(std::string());

   auto colon = node_name.find(':');
   if (colon IS std::string::npos) return XPathValue(std::string(node_name));
   return XPathValue(std::string(node_name.substr(colon + 1)));
}

XPathValue XPathFunctionLibrary::function_namespace_uri(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPVT::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue(std::string());

   if (target_attribute) {
      std::string_view name = target_attribute->Name;
      auto colon = name.find(':');
      if (colon IS std::string::npos) return XPathValue(std::string());

      std::string prefix(name.substr(0, colon));
      if (pf::iequals(prefix, "xml")) return XPathValue("http://www.w3.org/XML/1998/namespace");
      if (pf::iequals(prefix, "xmlns")) return XPathValue("http://www.w3.org/2000/xmlns/");

      XMLTag *scope_node = target_node ? target_node : Context.context_node;
      if (not scope_node) return XPathValue(std::string());

      if (Context.document) {
         std::string uri = find_in_scope_namespace(scope_node, Context.document, prefix);
         return XPathValue(uri);
      }

      return XPathValue(std::string());
   }

   if (not target_node) return XPathValue(std::string());

   std::string prefix;
   if (not target_node->Attribs.empty()) {
      std::string_view node_name = target_node->Attribs[0].Name;
      auto colon = node_name.find(':');
      if (colon != std::string::npos) prefix = std::string(node_name.substr(0, colon));
   }

   if (not prefix.empty()) {
      if (pf::iequals(prefix, "xml")) return XPathValue("http://www.w3.org/XML/1998/namespace");
      if (pf::iequals(prefix, "xmlns")) return XPathValue("http://www.w3.org/2000/xmlns/");
   }

   if ((target_node->NamespaceID != 0) and Context.document) {
      if (auto uri = Context.document->getNamespaceURI(target_node->NamespaceID)) return XPathValue(*uri);
   }

   if (Context.document) {
      std::string uri;
      if (not prefix.empty()) uri = find_in_scope_namespace(target_node, Context.document, prefix);
      else uri = find_in_scope_namespace(target_node, Context.document, std::string());
      return XPathValue(uri);
   }

   return XPathValue(std::string());
}

XPathValue XPathFunctionLibrary::function_name(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPVT::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue(std::string());

   if (target_attribute) return XPathValue(target_attribute->Name);

   if (not target_node) return XPathValue(std::string());
   if (target_node->Attribs.empty()) return XPathValue(std::string());

   return XPathValue(target_node->Attribs[0].Name);
}

