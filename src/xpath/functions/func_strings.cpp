//********************************************************************************************************************
// XPath String Functions

#include <format>

XPathValue XPathFunctionLibrary::function_string(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) {
      if (Context.attribute_node) {
         return XPathValue(Context.attribute_node->Value);
      }

      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         return XPathValue(node_set_value.to_string());
      }
      return XPathValue(std::string());
   }
   return XPathValue(Args[0].to_string());
}

XPathValue XPathFunctionLibrary::function_concat(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   // Pre-calculate total length to avoid quadratic behavior
   size_t total_length = 0;
   std::vector<std::string> arg_strings;
   arg_strings.reserve(Args.size());

   for (const auto &arg : Args) {
      arg_strings.emplace_back(arg.to_string());
      total_length += arg_strings.back().length();
   }

   std::string result;
   result.reserve(total_length);

   for (const auto &str : arg_strings) {
      result += str;
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_codepoints_to_string(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::string());

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);
   if (length IS 0u) return XPathValue(std::string());

   std::string output;
   output.reserve(length * 4u);

   for (size_t index = 0u; index < length; ++index) {
      XPathValue item = extract_sequence_item(sequence, index);
      double numeric = item.to_number();
      if (std::isnan(numeric)) continue;

      long long rounded = (long long)std::llround(numeric);
      if (rounded < 0) {
         append_codepoint_utf8(output, 0xFFFDu);
         continue;
      }

      append_codepoint_utf8(output, (uint32_t)rounded);
   }

   (void)Context;
   return XPathValue(output);
}

XPathValue XPathFunctionLibrary::function_string_to_codepoints(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());

   std::string input = Args[0].to_string();

   SequenceBuilder builder;
   size_t offset = 0u;

   while (offset < input.length()) {
      int length = 0;
      uint32_t code = UTF8ReadValue(input.c_str() + offset, &length);
      if (length <= 0) {
         code = (unsigned char)input[offset];
         length = 1;
      }

      builder.nodes.push_back(nullptr);
      builder.attributes.push_back(nullptr);
      builder.strings.push_back(std::to_string(code));

      offset += (size_t)length;
   }

   (void)Context;
   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_compare(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue();
   if (Args[0].is_empty() or Args[1].is_empty()) return XPathValue();

   std::string left = Args[0].to_string();
   std::string right = Args[1].to_string();
   std::string collation = (Args.size() > 2u) ? Args[2].to_string() : std::string();

   if (!collation.empty() and (collation != "http://www.w3.org/2005/xpath-functions/collation/codepoint") and
       (collation != "unicode")) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue();
   }

   int result = 0;
   if (left < right) result = -1;
   else if (left IS right) result = 0;
   else result = 1;

   return XPathValue((double)result);
}

XPathValue XPathFunctionLibrary::function_codepoint_equal(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue();
   if (Args[0].is_empty() or Args[1].is_empty()) return XPathValue();

   std::string first = Args[0].to_string();
   std::string second = Args[1].to_string();

   (void)Context;
   return XPathValue(first IS second);
}

XPathValue XPathFunctionLibrary::function_starts_with(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathValue(false);
   std::string str = Args[0].to_string();
   std::string prefix = Args[1].to_string();
   return XPathValue(str.substr(0, prefix.length()) IS prefix);
}

XPathValue XPathFunctionLibrary::function_ends_with(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2u) return XPathValue(false);

   std::string input = Args[0].to_string();
   std::string suffix = Args[1].to_string();
   if (suffix.length() > input.length()) return XPathValue(false);

   return XPathValue(input.compare(input.length() - suffix.length(), suffix.length(), suffix) IS 0);
}

XPathValue XPathFunctionLibrary::function_contains(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathValue(false);
   std::string str = Args[0].to_string();
   std::string substr = Args[1].to_string();
   return XPathValue(str.find(substr) != std::string::npos);
}

XPathValue XPathFunctionLibrary::function_substring_before(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathValue(std::string());

   std::string source = Args[0].to_string();
   std::string pattern = Args[1].to_string();

   if (pattern.empty()) return XPathValue(std::string());

   size_t position = source.find(pattern);
   if (position IS std::string::npos) return XPathValue(std::string());

   return XPathValue(source.substr(0, position));
}

XPathValue XPathFunctionLibrary::function_substring_after(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathValue(std::string());

   std::string source = Args[0].to_string();
   std::string pattern = Args[1].to_string();

   if (pattern.empty()) return XPathValue(source);

   size_t position = source.find(pattern);
   if (position IS std::string::npos) return XPathValue(std::string());

   return XPathValue(source.substr(position + pattern.length()));
}

XPathValue XPathFunctionLibrary::function_substring(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue(std::string());

   std::string str = Args[0].to_string();
   if (str.empty()) return XPathValue(std::string());

   double start_pos = Args[1].to_number();
   if (std::isnan(start_pos) or std::isinf(start_pos)) return XPathValue(std::string());

   // XPath uses 1-based indexing
   int start_index = (int)std::round(start_pos) - 1;
   if (start_index < 0) start_index = 0;
   if (start_index >= (int)str.length()) return XPathValue(std::string());

   if (Args.size() IS 3) {
      double length = Args[2].to_number();
      if (std::isnan(length) or std::isinf(length) or length <= 0) return XPathValue(std::string());

      int len = (int)std::round(length);
      int remaining = (int)str.length() - start_index;
      if (len > remaining) len = remaining;

      // For small substrings, avoid extra allocation overhead
      if (len <= 0) return XPathValue(std::string());

      return XPathValue(str.substr(start_index, len));
   }

   // Return substring from start_index to end
   return XPathValue(str.substr(start_index));
}

XPathValue XPathFunctionLibrary::function_string_length(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         str = node_set_value.to_string();
      }
   }
   else str = Args[0].to_string();
   return XPathValue((double)str.length());
}

XPathValue XPathFunctionLibrary::function_normalize_space(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         str = node_set_value.to_string();
      }
   }
   else str = Args[0].to_string();

   // Remove leading and trailing whitespace, collapse internal whitespace
   size_t start = str.find_first_not_of(" \t\n\r");
   if (start IS std::string::npos) return XPathValue(std::string());

   size_t end = str.find_last_not_of(" \t\n\r");
   str = str.substr(start, end - start + 1);

   // Collapse internal whitespace
   std::string result;
   result.reserve(estimate_normalize_space_size(str));
   bool in_whitespace = false;
   for (char c : str) {
      if (c IS ' ' or c IS '\t' or c IS '\n' or c IS '\r') {
         if (not in_whitespace) {
            result += ' ';
            in_whitespace = true;
         }
      }
      else {
         result += c;
         in_whitespace = false;
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_normalize_unicode(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::string());

   std::string input = Args[0].to_string();
   std::string form = (Args.size() > 1u) ? Args[1].to_string() : std::string("NFC");

   bool unsupported = false;
   std::string normalised = simple_normalise_unicode(input, form, &unsupported);
   if (unsupported and Context.expression_unsupported) *Context.expression_unsupported = true;

   return XPathValue(normalised);
}

XPathValue XPathFunctionLibrary::function_string_join(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::string());

   const XPathValue &sequence = Args[0];
   std::string separator = (Args.size() > 1u) ? Args[1].to_string() : std::string();

   size_t length = sequence_length(sequence);
   if (length IS 0u) return XPathValue(std::string());

   std::string result;
   for (size_t index = 0u; index < length; ++index) {
      if (index > 0u) result.append(separator);
      result.append(sequence_item_string(sequence, index));
   }

   (void)Context;
   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_translate(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 3) return XPathValue(std::string());

   std::string source = Args[0].to_string();
   std::string from = Args[1].to_string();
   std::string to = Args[2].to_string();

   if (source.empty()) return XPathValue(std::string());

   // Pre-size result string based on input length (worst case: no characters removed)
   std::string result;
   result.reserve(source.size());

   // Build character mapping for faster lookups if worth it
   if (from.size() > 10) {
      // Use array mapping for better performance with larger translation sets
      std::array<int, 256> char_map;
      char_map.fill(-1);

      for (size_t i = 0; i < from.size() and i < 256; ++i) {
         unsigned char ch = (unsigned char)from[i];
         if (char_map[ch] IS -1) { // First occurrence takes precedence
            char_map[ch] = (int)i;
         }
      }

      for (char ch : source) {
         unsigned char uch = (unsigned char)ch;
         int index = char_map[uch];
         if (index IS -1) result.push_back(ch);
         else if (index < (int)to.length()) result.push_back(to[index]);
      }
   }
   else {
      // Use simple find for small translation sets
      for (char ch : source) {
         size_t index = from.find(ch);
         if (index IS std::string::npos) result.push_back(ch);
         else if (index < to.length()) result.push_back(to[index]);
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_upper_case(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(apply_string_case(input, true));
}

XPathValue XPathFunctionLibrary::function_lower_case(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(apply_string_case(input, false));
}

XPathValue XPathFunctionLibrary::function_iri_to_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   std::string result;
   result.reserve(input.length() * 3u);

   static const char hex_digits[] = "0123456789ABCDEF";

   for (unsigned char code : input) {
      if (code <= 0x7Fu) {
         result.push_back((char)code);
      }
      else {
         result.push_back('%');
         result.push_back(hex_digits[(code >> 4u) & 0x0Fu]);
         result.push_back(hex_digits[code & 0x0Fu]);
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_encode_for_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(encode_for_uri_impl(input));
}

XPathValue XPathFunctionLibrary::function_escape_html_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(escape_html_uri_impl(input));
}

XPathValue XPathFunctionLibrary::function_matches(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue(false);

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() IS 3) ? Args[2].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathValue(false);
   }

   pf::MatchResult result;
   bool matched = compiled.search(input, result);
   return XPathValue(matched);
}

XPathValue XPathFunctionLibrary::function_replace(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 3 or Args.size() > 4) return XPathValue(std::string());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string replacement = Args[2].to_string();
   std::string flags = (Args.size() IS 4) ? Args[3].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathValue(input);
   }

   std::string replaced;
   if (not compiled.replace(input, replacement, replaced)) replaced = input;

   return XPathValue(replaced);
}

XPathValue XPathFunctionLibrary::function_tokenize(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue(std::vector<XMLTag *>());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() IS 3) ? Args[2].to_string() : std::string();

   std::vector<std::string> tokens;

   if (pattern.empty()) {
      for (size_t index = 0; index < input.length(); ++index) {
         tokens.emplace_back(input.substr(index, 1));
      }
   }
   else {
      pf::SyntaxOptions options = build_regex_options(flags, Context.expression_unsupported);

      pf::Regex compiled;
      if (not compiled.compile(pattern, options)) {
         return XPathValue(std::vector<XMLTag *>());
      }

      compiled.tokenize(input, -1, tokens);

      if (not tokens.empty() and tokens.back().empty()) tokens.pop_back();
   }

   std::vector<XMLTag *> placeholders(tokens.size(), nullptr);
   return XPathValue(placeholders, std::nullopt, tokens);
}

XPathValue XPathFunctionLibrary::function_analyze_string(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u or Args.size() > 3u) return XPathValue(std::vector<XMLTag *>());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() > 2u) ? Args[2].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathValue(std::vector<XMLTag *>());
   }

   SequenceBuilder builder;
   size_t search_offset = 0u;
   size_t guard = 0u;

   while (search_offset <= input.length()) {
      std::string_view remaining(input.c_str() + search_offset, input.length() - search_offset);
      pf::MatchResult match;
      if (!compiled.search(remaining, match)) {
         if (!remaining.empty()) {
            builder.nodes.push_back(nullptr);
            builder.attributes.push_back(nullptr);
            builder.strings.push_back(std::format("non-match:{}", remaining));
         }
         break;
      }

      if ((match.span.offset != (size_t)std::string::npos) and (match.span.offset > 0u)) {
         std::string unmatched = std::string(remaining.substr(0u, match.span.offset));
         if (!unmatched.empty()) {
            builder.nodes.push_back(nullptr);
            builder.attributes.push_back(nullptr);
            builder.strings.push_back(std::format("non-match:{}", unmatched));
         }
      }

      std::string matched_text;
      if (match.span.offset != (size_t)std::string::npos) {
         matched_text = std::string(remaining.substr(match.span.offset, match.span.length));
      }

      builder.nodes.push_back(nullptr);
      builder.attributes.push_back(nullptr);
      builder.strings.push_back(std::format("match:{}", matched_text));

      for (size_t index = 1u; index < match.captures.size(); ++index) {
         if (match.capture_spans[index].offset IS (size_t)std::string::npos) continue;
         builder.nodes.push_back(nullptr);
         builder.attributes.push_back(nullptr);
         builder.strings.push_back(std::format("group{}:{}", index, match.captures[index]));
      }

      size_t advance = 0u;
      if (match.span.offset != (size_t)std::string::npos) advance = match.span.offset;
      if (match.span.length > 0u) advance += match.span.length;
      else advance += 1u;

      search_offset += advance;

      guard++;
      if (guard > input.length() + 8u) break;
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_resolve_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   std::string relative = Args[0].to_string();
   std::string base;

   if (Args.size() > 1u and not Args[1].is_empty()) base = Args[1].to_string();
   else if (Context.document) base = Context.document->Path;

   if (relative.empty()) {
      if (base.empty()) return XPathValue();
      return XPathValue(base);
   }

   if (is_absolute_uri(relative)) return XPathValue(relative);
   if (base.empty()) return XPathValue();

   std::string resolved = resolve_relative_uri(relative, base);
   return XPathValue(resolved);
}

XPathValue XPathFunctionLibrary::function_format_date(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue(std::string());
   if (Args[0].is_empty()) return XPathValue();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_date_value(value, components)) return XPathValue(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathValue(formatted);
}

XPathValue XPathFunctionLibrary::function_format_time(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue(std::string());
   if (Args[0].is_empty()) return XPathValue();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_time_value(value, components)) return XPathValue(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathValue(formatted);
}

XPathValue XPathFunctionLibrary::function_format_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue(std::string());
   if (Args[0].is_empty()) return XPathValue();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_date_time_components(value, components)) return XPathValue(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathValue(formatted);
}

XPathValue XPathFunctionLibrary::function_format_integer(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue(std::string());

   double number = Args[0].to_number();
   if (std::isnan(number) or std::isinf(number)) return XPathValue(std::string());

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   long long rounded = (long long)std::llround(number);
   std::string picture = Args[1].to_string();
   std::string formatted = format_integer_picture(rounded, picture);
   return XPathValue(formatted);
}

