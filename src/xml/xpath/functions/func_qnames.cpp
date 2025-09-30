//********************************************************************************************************************
// XPath QName Functions

namespace {

struct CanonicalQName
{
   bool valid = false;
   std::string prefix;
   std::string local_name;
   std::string namespace_uri;
};

static bool is_ncname_start(char Ch)
{
   if (Ch IS ':') return false;
   return is_name_start(Ch);
}

static bool is_ncname_char(char Ch)
{
   if (Ch IS ':') return false;
   return is_name_char(Ch);
}

static bool is_valid_ncname(std::string_view Value)
{
   if (Value.empty()) return false;
   if (not is_ncname_start(Value.front())) return false;

   for (size_t index = 1; index < Value.length(); ++index) {
      if (not is_ncname_char(Value[index])) return false;
   }

   return true;
}

static std::string trim_whitespace(std::string_view Value)
{
   size_t start = 0;
   size_t end = Value.length();

   while ((start < end) and (unsigned char)Value[start] <= 0x20u) ++start;
   while ((end > start) and (unsigned char)Value[end - 1] <= 0x20u) --end;

   return std::string(Value.substr(start, end - start));
}

static bool parse_lexical_qname(std::string_view Value, std::string &Prefix, std::string &Local)
{
   if (Value.empty()) return false;

   size_t colon = Value.find(':');
   if (colon IS std::string::npos) {
      if (not is_valid_ncname(Value)) return false;
      Prefix.clear();
      Local.assign(Value);
      return true;
   }

   std::string_view prefix_view = Value.substr(0, colon);
   std::string_view local_view = Value.substr(colon + 1);
   if (prefix_view.empty() or local_view.empty()) return false;
   if (not is_valid_ncname(prefix_view) or not is_valid_ncname(local_view)) return false;

   Prefix.assign(prefix_view);
   Local.assign(local_view);
   return true;
}

static CanonicalQName decode_qname_string(const std::string &Value)
{
   CanonicalQName result;
   std::string_view view(Value);

   if (view.empty()) return result;

   if ((view.length() >= 2) and (view[0] IS 'Q') and (view[1] IS '{')) {
      size_t closing = view.find('}');
      if (closing IS std::string::npos) return result;

      result.namespace_uri = std::string(view.substr(2, closing - 2));
      std::string_view remainder = view.substr(closing + 1);
      if (remainder.empty()) return result;

      size_t colon = remainder.find(':');
      if (colon IS std::string::npos) {
         if (not is_valid_ncname(remainder)) return result;
         result.prefix.clear();
         result.local_name = std::string(remainder);
      }
      else {
         std::string_view prefix_view = remainder.substr(0, colon);
         std::string_view local_view = remainder.substr(colon + 1);
         if (local_view.empty()) return result;
         if ((not prefix_view.empty()) and (not is_valid_ncname(prefix_view))) return result;
         if (not is_valid_ncname(local_view)) return result;
         result.prefix = std::string(prefix_view);
         result.local_name = std::string(local_view);
      }

      result.valid = true;
      return result;
   }

   std::string prefix;
   std::string local;
   if (not parse_lexical_qname(view, prefix, local)) return result;

   result.valid = true;
   result.prefix = std::move(prefix);
   result.local_name = std::move(local);
   result.namespace_uri.clear();
   return result;
}

static std::string encode_canonical_qname(const std::string &NamespaceURI, const std::string &Prefix, const std::string &Local)
{
   std::string result("Q{");
   result.append(NamespaceURI);
   result.push_back('}');
   if (not Prefix.empty()) {
      result.append(Prefix);
      result.push_back(':');
   }
   result.append(Local);
   return result;
}

static std::string find_namespace_for_prefix(XMLTag *Node, extXML *Document, const std::string &Prefix)
{
   if (pf::iequals(Prefix, "xml")) return std::string("http://www.w3.org/XML/1998/namespace");
   if (pf::iequals(Prefix, "xmlns")) return std::string("http://www.w3.org/2000/xmlns/");

   if (not Document) return std::string();
   return find_in_scope_namespace(Node, Document, Prefix);
}

static std::vector<std::string> collect_in_scope_prefixes(XMLTag *Node, extXML *Document)
{
   std::vector<std::string> prefixes;
   std::unordered_set<std::string> seen;
   bool default_found = false;

   if ((not Node) or (not Document)) {
      prefixes.push_back("xml");
      return prefixes;
   }

   XMLTag *current = Node;
   while (current) {
      for (size_t index = 1; index < current->Attribs.size(); ++index) {
         const auto &attrib = current->Attribs[index];
         if (attrib.Name.compare(0, 6, "xmlns:") IS 0) {
            std::string declared = attrib.Name.substr(6);
            if (seen.insert(declared).second) prefixes.push_back(declared);
         }
         else if (attrib.Name.compare("xmlns") IS 0) {
            default_found = true;
            if (seen.insert(std::string()).second) prefixes.push_back(std::string());
         }
      }

      if (current->ParentID IS 0) break;
      current = Document->getTag(current->ParentID);
   }

   if (Document) {
      if (not default_found) {
         std::string default_namespace = find_in_scope_namespace(Node, Document, std::string());
         if (not default_namespace.empty()) {
            seen.insert(std::string());
            prefixes.push_back(std::string());
         }
      }
   }

   if (seen.insert("xml").second) prefixes.push_back("xml");

   return prefixes;
}

} // namespace

XPathValue XPathFunctionLibrary::function_QName(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(std::vector<XMLTag *>());

   std::string namespace_uri;
   if (not Args[0].is_empty()) namespace_uri = trim_whitespace(Args[0].to_string());

   if (Args[1].is_empty()) return XPathValue(std::vector<XMLTag *>());
   std::string lexical = trim_whitespace(Args[1].to_string());
   std::string prefix;
   std::string local;
   if (not parse_lexical_qname(lexical, prefix, local)) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(std::vector<XMLTag *>());
   }

   if ((not prefix.empty()) and namespace_uri.empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(std::vector<XMLTag *>());
   }

   std::string encoded = encode_canonical_qname(namespace_uri, prefix, local);
   return XPathValue(std::move(encoded));
}

XPathValue XPathFunctionLibrary::function_resolve_QName(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(std::vector<XMLTag *>());
   if (Args[0].is_empty()) return XPathValue(std::vector<XMLTag *>());

   std::string lexical = trim_whitespace(Args[0].to_string());
   std::string prefix;
   std::string local;
   if (not parse_lexical_qname(lexical, prefix, local)) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(std::vector<XMLTag *>());
   }

   XMLTag *element_node = nullptr;
   if (Args[1].type IS XPathValueType::NodeSet) {
      if (not Args[1].node_set.empty()) element_node = Args[1].node_set[0];
   }

   if (not element_node) element_node = Context.context_node;
   if ((not element_node) or (not element_node->isTag())) return XPathValue(std::vector<XMLTag *>());

   std::string namespace_uri;
   if (prefix.empty()) namespace_uri = find_in_scope_namespace(element_node, Context.document, std::string());
   else namespace_uri = find_namespace_for_prefix(element_node, Context.document, prefix);

   if ((not prefix.empty()) and namespace_uri.empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(std::vector<XMLTag *>());
   }

   std::string encoded = encode_canonical_qname(namespace_uri, prefix, local);
   return XPathValue(std::move(encoded));
}

XPathValue XPathFunctionLibrary::function_prefix_from_QName(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
   if (Args[0].is_empty()) return XPathValue(std::vector<XMLTag *>());

   CanonicalQName qname = decode_qname_string(Args[0].to_string());
   if (not qname.valid) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(std::vector<XMLTag *>());
   }

   if (qname.prefix.empty()) return XPathValue(std::vector<XMLTag *>());
   return XPathValue(qname.prefix);
}

XPathValue XPathFunctionLibrary::function_local_name_from_QName(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
   if (Args[0].is_empty()) return XPathValue(std::vector<XMLTag *>());

   CanonicalQName qname = decode_qname_string(Args[0].to_string());
   if (not qname.valid) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(std::vector<XMLTag *>());
   }

   if (qname.local_name.empty()) return XPathValue(std::vector<XMLTag *>());
   return XPathValue(qname.local_name);
}

XPathValue XPathFunctionLibrary::function_namespace_uri_from_QName(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
   if (Args[0].is_empty()) return XPathValue(std::vector<XMLTag *>());

   CanonicalQName qname = decode_qname_string(Args[0].to_string());
   if (not qname.valid) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue(std::vector<XMLTag *>());
   }

   if (qname.namespace_uri.empty()) return XPathValue(std::vector<XMLTag *>());
   return XPathValue(qname.namespace_uri);
}

XPathValue XPathFunctionLibrary::function_namespace_uri_for_prefix(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(std::vector<XMLTag *>());

   std::string prefix;
   if (not Args[0].is_empty()) prefix = trim_whitespace(Args[0].to_string());

   XMLTag *element_node = nullptr;
   if (Args[1].type IS XPathValueType::NodeSet) {
      if (not Args[1].node_set.empty()) element_node = Args[1].node_set[0];
   }

   if (not element_node) element_node = Context.context_node;
   if ((not element_node) or (not element_node->isTag())) return XPathValue(std::vector<XMLTag *>());

   std::string namespace_uri;
   if (prefix.empty()) namespace_uri = find_in_scope_namespace(element_node, Context.document, std::string());
   else namespace_uri = find_namespace_for_prefix(element_node, Context.document, prefix);

   if (namespace_uri.empty()) return XPathValue(std::vector<XMLTag *>());
   return XPathValue(namespace_uri);
}

XPathValue XPathFunctionLibrary::function_in_scope_prefixes(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   XMLTag *element_node = nullptr;
   if (not Args.empty() and (Args[0].type IS XPathValueType::NodeSet)) {
      if (not Args[0].node_set.empty()) element_node = Args[0].node_set[0];
   }

   if (not element_node) element_node = Context.context_node;
   if ((not element_node) or (not element_node->isTag())) {
      SequenceBuilder builder;
      append_value_to_sequence(XPathValue("xml"), builder);
      return make_sequence_value(std::move(builder));
   }

   std::vector<std::string> prefixes = collect_in_scope_prefixes(element_node, Context.document);
   if (Context.document) {
      auto missing_default = std::find(prefixes.begin(), prefixes.end(), std::string()) == prefixes.end();
      if (missing_default) {
         std::string default_namespace = find_in_scope_namespace(element_node, Context.document, std::string());
         if (not default_namespace.empty()) prefixes.push_back(std::string());
      }
   }
   SequenceBuilder builder;
   for (const auto &prefix : prefixes) {
      append_value_to_sequence(XPathValue(prefix), builder);
   }

   return make_sequence_value(std::move(builder));
}

