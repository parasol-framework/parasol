//********************************************************************************************************************
// XPath String Functions

#include <format>

XPathVal XPathFunctionLibrary::function_string(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.empty()) {
      if (Context.attribute_node) {
         return XPathVal(Context.attribute_node->Value);
      }

      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathVal node_set_value(nodes);
         return XPathVal(node_set_value.to_string());
      }
      return XPathVal(std::string());
   }
   return XPathVal(Args[0].to_string());
}

XPathVal XPathFunctionLibrary::function_concat(const std::vector<XPathVal> &Args, const XPathContext &Context)
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

   return XPathVal(result);
}

XPathVal XPathFunctionLibrary::function_codepoints_to_string(const std::vector<XPathVal> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathVal(std::string());

   const XPathVal &sequence = Args[0];
   size_t length = sequence_length(sequence);
   if (length IS 0u) return XPathVal(std::string());

   std::string output;
   output.reserve(length * 4u);

   for (size_t index = 0u; index < length; ++index) {
      XPathVal item = extract_sequence_item(sequence, index);
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
   return XPathVal(output);
}

XPathVal XPathFunctionLibrary::function_string_to_codepoints(const std::vector<XPathVal> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathVal(std::vector<XMLTag *>());

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

XPathVal XPathFunctionLibrary::function_compare(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathVal();
   if (Args[0].is_empty() or Args[1].is_empty()) return XPathVal();

   std::string left = Args[0].to_string();
   std::string right = Args[1].to_string();
   std::string collation = (Args.size() > 2u) ? Args[2].to_string() : std::string();

   if (!collation.empty() and (collation != "http://www.w3.org/2005/xpath-functions/collation/codepoint") and
       (collation != "unicode")) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathVal();
   }

   int result = 0;
   if (left < right) result = -1;
   else if (left IS right) result = 0;
   else result = 1;

   return XPathVal((double)result);
}

XPathVal XPathFunctionLibrary::function_codepoint_equal(const std::vector<XPathVal> &Args,
   const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathVal();
   if (Args[0].is_empty() or Args[1].is_empty()) return XPathVal();

   std::string first = Args[0].to_string();
   std::string second = Args[1].to_string();

   (void)Context;
   return XPathVal(first IS second);
}

XPathVal XPathFunctionLibrary::function_starts_with(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathVal(false);
   std::string str = Args[0].to_string();
   std::string prefix = Args[1].to_string();
   return XPathVal(str.substr(0, prefix.length()) IS prefix);
}

XPathVal XPathFunctionLibrary::function_ends_with(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() != 2u) return XPathVal(false);

   std::string input = Args[0].to_string();
   std::string suffix = Args[1].to_string();
   if (suffix.length() > input.length()) return XPathVal(false);

   return XPathVal(input.compare(input.length() - suffix.length(), suffix.length(), suffix) IS 0);
}

XPathVal XPathFunctionLibrary::function_contains(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathVal(false);
   std::string str = Args[0].to_string();
   std::string substr = Args[1].to_string();
   return XPathVal(str.find(substr) != std::string::npos);
}

XPathVal XPathFunctionLibrary::function_substring_before(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathVal(std::string());

   std::string source = Args[0].to_string();
   std::string pattern = Args[1].to_string();

   if (pattern.empty()) return XPathVal(std::string());

   size_t position = source.find(pattern);
   if (position IS std::string::npos) return XPathVal(std::string());

   return XPathVal(source.substr(0, position));
}

XPathVal XPathFunctionLibrary::function_substring_after(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathVal(std::string());

   std::string source = Args[0].to_string();
   std::string pattern = Args[1].to_string();

   if (pattern.empty()) return XPathVal(source);

   size_t position = source.find(pattern);
   if (position IS std::string::npos) return XPathVal(std::string());

   return XPathVal(source.substr(position + pattern.length()));
}

XPathVal XPathFunctionLibrary::function_substring(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathVal(std::string());

   std::string str = Args[0].to_string();
   if (str.empty()) return XPathVal(std::string());

   double start_pos = Args[1].to_number();
   if (std::isnan(start_pos) or std::isinf(start_pos)) return XPathVal(std::string());

   // XPath uses 1-based indexing
   int start_index = (int)std::round(start_pos) - 1;
   if (start_index < 0) start_index = 0;
   if (start_index >= (int)str.length()) return XPathVal(std::string());

   if (Args.size() IS 3) {
      double length = Args[2].to_number();
      if (std::isnan(length) or std::isinf(length) or length <= 0) return XPathVal(std::string());

      int len = (int)std::round(length);
      int remaining = (int)str.length() - start_index;
      if (len > remaining) len = remaining;

      // For small substrings, avoid extra allocation overhead
      if (len <= 0) return XPathVal(std::string());

      return XPathVal(str.substr(start_index, len));
   }

   // Return substring from start_index to end
   return XPathVal(str.substr(start_index));
}

XPathVal XPathFunctionLibrary::function_string_length(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathVal node_set_value(nodes);
         str = node_set_value.to_string();
      }
   }
   else str = Args[0].to_string();
   return XPathVal((double)str.length());
}

XPathVal XPathFunctionLibrary::function_normalize_space(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathVal node_set_value(nodes);
         str = node_set_value.to_string();
      }
   }
   else str = Args[0].to_string();

   // Remove leading and trailing whitespace, collapse internal whitespace
   size_t start = str.find_first_not_of(" \t\n\r");
   if (start IS std::string::npos) return XPathVal(std::string());

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

   return XPathVal(result);
}

XPathVal XPathFunctionLibrary::function_normalize_unicode(const std::vector<XPathVal> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathVal(std::string());

   std::string input = Args[0].to_string();
   std::string form = (Args.size() > 1u) ? Args[1].to_string() : std::string("NFC");

   bool unsupported = false;
   std::string normalised = simple_normalise_unicode(input, form, &unsupported);
   if (unsupported and Context.expression_unsupported) *Context.expression_unsupported = true;

   return XPathVal(normalised);
}

XPathVal XPathFunctionLibrary::function_string_join(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathVal(std::string());

   const XPathVal &sequence = Args[0];
   std::string separator = (Args.size() > 1u) ? Args[1].to_string() : std::string();

   size_t length = sequence_length(sequence);
   if (length IS 0u) return XPathVal(std::string());

   std::string result;
   for (size_t index = 0u; index < length; ++index) {
      if (index > 0u) result.append(separator);
      result.append(sequence_item_string(sequence, index));
   }

   (void)Context;
   return XPathVal(result);
}

XPathVal XPathFunctionLibrary::function_translate(const std::vector<XPathVal> &Args, const XPathContext &Context) {
   if (Args.size() != 3) return XPathVal(std::string());

   std::string source = Args[0].to_string();
   std::string from = Args[1].to_string();
   std::string to = Args[2].to_string();

   if (source.empty()) return XPathVal(std::string());

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

   return XPathVal(result);
}

XPathVal XPathFunctionLibrary::function_upper_case(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathVal node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathVal(apply_string_case(input, true));
}

XPathVal XPathFunctionLibrary::function_lower_case(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathVal node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathVal(apply_string_case(input, false));
}

XPathVal XPathFunctionLibrary::function_iri_to_uri(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathVal node_value(nodes);
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

   return XPathVal(result);
}

XPathVal XPathFunctionLibrary::function_encode_for_uri(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathVal node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathVal(encode_for_uri_impl(input));
}

XPathVal XPathFunctionLibrary::function_escape_html_uri(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathVal node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathVal(escape_html_uri_impl(input));
}

XPathVal XPathFunctionLibrary::function_matches(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathVal(false);

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() IS 3) ? Args[2].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathVal(false);
   }

   pf::MatchResult result;
   bool matched = compiled.search(input, result);
   return XPathVal(matched);
}

XPathVal XPathFunctionLibrary::function_replace(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 3 or Args.size() > 4) return XPathVal(std::string());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string replacement = Args[2].to_string();
   std::string flags = (Args.size() IS 4) ? Args[3].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathVal(input);
   }

   std::string replaced;
   if (not compiled.replace(input, replacement, replaced)) replaced = input;

   return XPathVal(replaced);
}

XPathVal XPathFunctionLibrary::function_tokenize(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathVal(std::vector<XMLTag *>());

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
         return XPathVal(std::vector<XMLTag *>());
      }

      compiled.tokenize(input, -1, tokens);

      if (not tokens.empty() and tokens.back().empty()) tokens.pop_back();
   }

   std::vector<XMLTag *> placeholders(tokens.size(), nullptr);
   return XPathVal(placeholders, std::nullopt, tokens);
}

XPathVal XPathFunctionLibrary::function_analyze_string(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u or Args.size() > 3u) return XPathVal(std::vector<XMLTag *>());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() > 2u) ? Args[2].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathVal(std::vector<XMLTag *>());
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

XPathVal XPathFunctionLibrary::function_resolve_uri(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathVal();

   std::string relative = Args[0].to_string();
   std::string base;

   if (Args.size() > 1u and not Args[1].is_empty()) base = Args[1].to_string();
   else if (Context.document) base = Context.document->Path;

   if (relative.empty()) {
      if (base.empty()) return XPathVal();
      return XPathVal(base);
   }

   if (is_absolute_uri(relative)) return XPathVal(relative);
   if (base.empty()) return XPathVal();

   std::string resolved = resolve_relative_uri(relative, base);
   return XPathVal(resolved);
}

XPathVal XPathFunctionLibrary::function_format_date(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathVal(std::string());
   if (Args[0].is_empty()) return XPathVal();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_date_value(value, components)) return XPathVal(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathVal(formatted);
}

XPathVal XPathFunctionLibrary::function_format_time(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathVal(std::string());
   if (Args[0].is_empty()) return XPathVal();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_time_value(value, components)) return XPathVal(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathVal(formatted);
}

XPathVal XPathFunctionLibrary::function_format_date_time(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathVal(std::string());
   if (Args[0].is_empty()) return XPathVal();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_date_time_components(value, components)) return XPathVal(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathVal(formatted);
}

XPathVal XPathFunctionLibrary::function_format_integer(const std::vector<XPathVal> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathVal(std::string());

   double number = Args[0].to_number();
   if (std::isnan(number) or std::isinf(number)) return XPathVal(std::string());

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   long long rounded = (long long)std::llround(number);
   std::string picture = Args[1].to_string();
   std::string formatted = format_integer_picture(rounded, picture);
   return XPathVal(formatted);
}

