// XPathFunctionLibrary Implementation

//********************************************************************************************************************
// Constructs the function library and registers all core XPath functions.

XPathFunctionLibrary::XPathFunctionLibrary() {
   register_core_functions();
}

//********************************************************************************************************************
// Returns the singleton instance of the XPathFunctionLibrary, creating it on first call using call_once for
// thread-safe initialisation.

const XPathFunctionLibrary & XPathFunctionLibrary::instance()
{
   static std::once_flag initialise_flag;
   static std::unique_ptr<XPathFunctionLibrary> shared_library;

   std::call_once(initialise_flag, [&]() {
      shared_library = std::unique_ptr<XPathFunctionLibrary>(new XPathFunctionLibrary());
   });

   return *shared_library;
}

void XPathFunctionLibrary::register_core_functions() {
   // Node Set Functions
   register_function("last", function_last);
   register_function("position", function_position);
   register_function("count", function_count);
   register_function("id", function_id);
   register_function("idref", function_idref);
   register_function("root", function_root);

   // Document Functions
   register_function("doc", function_doc);
   register_function("doc-available", function_doc_available);
   register_function("collection", function_collection);
   register_function("uri-collection", function_uri_collection);
   register_function("unparsed-text", function_unparsed_text);
   register_function("unparsed-text-available", function_unparsed_text_available);
   register_function("unparsed-text-lines", function_unparsed_text_lines);
   register_function("local-name", function_local_name);
   register_function("namespace-uri", function_namespace_uri);
   register_function("name", function_name);

   // Accessor Functions
   register_function("base-uri", function_base_uri);
   register_function("data", function_data);
   register_function("document-uri", function_document_uri);
   register_function("node-name", function_node_name);
   register_function("nilled", function_nilled);
   register_function("static-base-uri", function_static_base_uri);
   register_function("default-collation", function_default_collation);

   // QName Functions
   register_function("QName", function_QName);
   register_function("resolve-QName", function_resolve_QName);
   register_function("prefix-from-QName", function_prefix_from_QName);
   register_function("local-name-from-QName", function_local_name_from_QName);
   register_function("namespace-uri-from-QName", function_namespace_uri_from_QName);
   register_function("namespace-uri-for-prefix", function_namespace_uri_for_prefix);
   register_function("in-scope-prefixes", function_in_scope_prefixes);

   // String Functions
   register_function("string", function_string);
   register_function("concat", function_concat);
   register_function("codepoints-to-string", function_codepoints_to_string);
   register_function("string-to-codepoints", function_string_to_codepoints);
   register_function("compare", function_compare);
   register_function("codepoint-equal", function_codepoint_equal);
   register_function("starts-with", function_starts_with);
   register_function("ends-with", function_ends_with);
   register_function("contains", function_contains);
   register_function("substring-before", function_substring_before);
   register_function("substring-after", function_substring_after);
   register_function("substring", function_substring);
   register_function("string-length", function_string_length);
   register_function("normalize-space", function_normalize_space);
   register_function("normalize-unicode", function_normalize_unicode);
   register_function("string-join", function_string_join);
   register_function("iri-to-uri", function_iri_to_uri);
   register_function("translate", function_translate);
   register_function("upper-case", function_upper_case);
   register_function("lower-case", function_lower_case);
   register_function("encode-for-uri", function_encode_for_uri);
   register_function("escape-html-uri", function_escape_html_uri);

   register_function("matches", function_matches);
   register_function("replace", function_replace);
   register_function("tokenize", function_tokenize);
   register_function("analyze-string", function_analyze_string);
   register_function("resolve-uri", function_resolve_uri);
   register_function("format-date", function_format_date);
   register_function("format-time", function_format_time);
   register_function("format-dateTime", function_format_date_time);
   register_function("format-integer", function_format_integer);

   // Diagnostics Functions
   register_function("error", function_error);
   register_function("trace", function_trace);

   // Boolean Functions
   register_function("boolean", function_boolean);
   register_function("not", function_not);
   register_function("true", function_true);
   register_function("false", function_false);
   register_function("lang", function_lang);
   register_function("exists", function_exists);

   // Sequence Functions
   register_function("index-of", function_index_of);
   register_function("empty", function_empty);
   register_function("distinct-values", function_distinct_values);
   register_function("insert-before", function_insert_before);
   register_function("remove", function_remove);
   register_function("reverse", function_reverse);
   register_function("subsequence", function_subsequence);
   register_function("unordered", function_unordered);
   register_function("deep-equal", function_deep_equal);
   register_function("zero-or-one", function_zero_or_one);
   register_function("one-or-more", function_one_or_more);
   register_function("exactly-one", function_exactly_one);

   // Number Functions
   register_function("number", function_number);
   register_function("sum", function_sum);
   register_function("floor", function_floor);
   register_function("ceiling", function_ceiling);
   register_function("round", function_round);
   register_function("round-half-to-even", function_round_half_to_even);
   register_function("abs", function_abs);
   register_function("min", function_min);
   register_function("max", function_max);
   register_function("avg", function_avg);

   // Date and Time Functions
   register_function("current-date", function_current_date);
   register_function("current-time", function_current_time);
   register_function("current-dateTime", function_current_date_time);
   register_function("dateTime", function_date_time);
   register_function("year-from-dateTime", function_year_from_date_time);
   register_function("month-from-dateTime", function_month_from_date_time);
   register_function("day-from-dateTime", function_day_from_date_time);
   register_function("hours-from-dateTime", function_hours_from_date_time);
   register_function("minutes-from-dateTime", function_minutes_from_date_time);
   register_function("seconds-from-dateTime", function_seconds_from_date_time);
   register_function("timezone-from-dateTime", function_timezone_from_date_time);
   register_function("year-from-date", function_year_from_date);
   register_function("month-from-date", function_month_from_date);
   register_function("day-from-date", function_day_from_date);
   register_function("timezone-from-date", function_timezone_from_date);
   register_function("hours-from-time", function_hours_from_time);
   register_function("minutes-from-time", function_minutes_from_time);
   register_function("seconds-from-time", function_seconds_from_time);
   register_function("timezone-from-time", function_timezone_from_time);
   register_function("adjust-dateTime-to-timezone", function_adjust_date_time_to_timezone);
   register_function("adjust-date-to-timezone", function_adjust_date_to_timezone);
   register_function("adjust-time-to-timezone", function_adjust_time_to_timezone);
   register_function("implicit-timezone", function_implicit_timezone);
   register_function("years-from-duration", function_years_from_duration);
   register_function("months-from-duration", function_months_from_duration);
   register_function("days-from-duration", function_days_from_duration);
   register_function("hours-from-duration", function_hours_from_duration);
   register_function("minutes-from-duration", function_minutes_from_duration);
   register_function("seconds-from-duration", function_seconds_from_duration);
   register_function("years-from-yearMonthDuration", function_years_from_year_month_duration);
   register_function("months-from-yearMonthDuration", function_months_from_year_month_duration);
   register_function("days-from-dayTimeDuration", function_days_from_day_time_duration);
   register_function("hours-from-dayTimeDuration", function_hours_from_day_time_duration);
   register_function("minutes-from-dayTimeDuration", function_minutes_from_day_time_duration);
   register_function("seconds-from-dayTimeDuration", function_seconds_from_day_time_duration);
}

//********************************************************************************************************************
// Checks whether a function with the given name is registered in the library. Returns true if found; false
// otherwise.

bool XPathFunctionLibrary::has_function(std::string_view Name) const {
   return find_function(Name) != nullptr;
}

//********************************************************************************************************************
// Invokes a registered function by name with the provided arguments and context. Sets expression_unsupported and
// appends an error message if the function is not found.

XPathVal XPathFunctionLibrary::call_function(std::string_view Name, const std::vector<XPathVal> &Args,
   const XPathContext &Context) const {
   if (const auto *function_ptr = find_function(Name)) {
      return (*function_ptr)(Args, Context);
   }

   *Context.expression_unsupported = true;

   if (Context.document) {
      if (not Context.document->ErrorMsg.empty()) Context.document->ErrorMsg.append("\n");
      Context.document->ErrorMsg.append("Unsupported XPath function: ").append(Name);
   }

   return XPathVal();
}

//********************************************************************************************************************
// Registers a function implementation in the library map under the given name. Replaces any existing function
// with the same name.

void XPathFunctionLibrary::register_function(std::string_view Name, XPathFunction Func) {
   functions.insert_or_assign(std::string(Name), std::move(Func));
}

//********************************************************************************************************************
// Looks up a function by name and returns a pointer to its implementation if found; nullptr otherwise.

const XPathFunction * XPathFunctionLibrary::find_function(std::string_view Name) const {
   auto iter = functions.find(Name);
   if (iter != functions.end()) return &iter->second;

   return nullptr;
}

//********************************************************************************************************************
// Estimates the buffer size needed for concatenating the string representations of all arguments. Uses type-based
// heuristics (e.g., 32 bytes for numbers, 5 for "false") for conservative overestimation.

size_t XPathFunctionLibrary::estimate_concat_size(const std::vector<XPathVal> &Args) {
   size_t total = 0;
   for (const auto &arg : Args) {
      // Conservative estimate based on type
      switch (arg.Type) {
         case XPVT::String:
         case XPVT::Date:
         case XPVT::Time:
         case XPVT::DateTime:
            total += arg.StringValue.length();
            break;
         case XPVT::Number:
            total += 32; // Conservative estimate for number formatting
            break;
         case XPVT::Boolean:
            total += 5; // "false" is longest
            break;
         case XPVT::NodeSet:
            if (arg.node_set_string_override.has_value()) {
               total += arg.node_set_string_override->length();
            } else if (not arg.node_set_string_values.empty()) {
               total += arg.node_set_string_values[0].length();
            } else {
               total += 64; // Conservative estimate for node content
            }
            break;
      }
   }
   return total;
}

//********************************************************************************************************************
// Estimates the output size for normalize-space operation, returning input length as worst case (no collapsing).

size_t XPathFunctionLibrary::estimate_normalize_space_size(const std::string &Input) {
   // Worst case: no whitespace collapsing needed
   return Input.length();
}

//********************************************************************************************************************
// Estimates the output size for translate operation, returning source length as worst case (no characters removed).

size_t XPathFunctionLibrary::estimate_translate_size(const std::string &Source, const std::string &From) {
   // Best case: no characters removed, worst case: same size as source
   return Source.length();
}
