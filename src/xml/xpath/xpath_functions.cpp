// XPath Function Library and Value System Implementation

//********************************************************************************************************************
// XPathValue Implementation

bool XPathValue::to_boolean() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value;
      case XPathValueType::Number: return number_value != 0.0 and !std::isnan(number_value);
      case XPathValueType::String: return !string_value.empty();
      case XPathValueType::NodeSet: return !node_set.empty();
   }
   return false;
}

double XPathValue::to_number() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value ? 1.0 : 0.0;
      case XPathValueType::Number: return number_value;
      case XPathValueType::String: {
         if (string_value.empty()) return std::numeric_limits<double>::quiet_NaN();
         char* end_ptr = nullptr;
         double result = std::strtod(string_value.c_str(), &end_ptr);
         if (end_ptr IS string_value.c_str() or *end_ptr != '\0') {
            return std::numeric_limits<double>::quiet_NaN();
         }
         return result;
      }
      case XPathValueType::NodeSet: {
         if (node_set.empty()) return std::numeric_limits<double>::quiet_NaN();
         std::string str = to_string();
         if (str.empty()) return std::numeric_limits<double>::quiet_NaN();
         char* end_ptr = nullptr;
         double result = std::strtod(str.c_str(), &end_ptr);
         if (end_ptr IS str.c_str() or *end_ptr != '\0') {
            return std::numeric_limits<double>::quiet_NaN();
         }
         return result;
      }
   }
   return 0.0;
}

std::string XPathValue::to_string() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value ? "true" : "false";
      case XPathValueType::Number: {
         if (std::isnan(number_value)) return "NaN";
         if (std::isinf(number_value)) return number_value > 0 ? "Infinity" : "-Infinity";
         if (number_value IS std::floor(number_value)) {
            return std::to_string((long long)number_value);
         }
         return std::to_string(number_value);
      }
      case XPathValueType::String: return string_value;
      case XPathValueType::NodeSet: {
         if (node_set.empty()) return "";
         XMLTag *tag = node_set[0];
         if (tag) {
            return tag->getContent();
         }
         return "";
      }
   }
   return "";
}

std::vector<XMLTag *> XPathValue::to_node_set() const {
   if (type IS XPathValueType::NodeSet) {
      return node_set;
   }
   return {};
}

bool XPathValue::is_empty() const {
   switch (type) {
      case XPathValueType::Boolean: return false;
      case XPathValueType::Number: return false;
      case XPathValueType::String: return string_value.empty();
      case XPathValueType::NodeSet: return node_set.empty();
   }
   return true;
}

size_t XPathValue::size() const {
   switch (type) {
      case XPathValueType::NodeSet: return node_set.size();
      default: return is_empty() ? 0 : 1;
   }
}

//********************************************************************************************************************
// XPathFunctionLibrary Implementation

XPathFunctionLibrary::XPathFunctionLibrary() {
   register_core_functions();
}

void XPathFunctionLibrary::register_core_functions() {
   // Node Set Functions
   register_function("last", function_last);
   register_function("position", function_position);
   register_function("count", function_count);
   register_function("id", function_id);
   register_function("local-name", function_local_name);
   register_function("namespace-uri", function_namespace_uri);
   register_function("name", function_name);

   // String Functions
   register_function("string", function_string);
   register_function("concat", function_concat);
   register_function("starts-with", function_starts_with);
   register_function("contains", function_contains);
   register_function("substring-before", function_substring_before);
   register_function("substring-after", function_substring_after);
   register_function("substring", function_substring);
   register_function("string-length", function_string_length);
   register_function("normalize-space", function_normalize_space);
   register_function("translate", function_translate);

   // Boolean Functions
   register_function("boolean", function_boolean);
   register_function("not", function_not);
   register_function("true", function_true);
   register_function("false", function_false);
   register_function("lang", function_lang);

   // Number Functions
   register_function("number", function_number);
   register_function("sum", function_sum);
   register_function("floor", function_floor);
   register_function("ceiling", function_ceiling);
   register_function("round", function_round);
}

bool XPathFunctionLibrary::has_function(const std::string &Name) const {
   return functions.find(Name) != functions.end();
}

XPathValue XPathFunctionLibrary::call_function(const std::string &Name,
                                               const std::vector<XPathValue> &Args,
                                               const XPathContext &Context) const {
   auto it = functions.find(Name);
   if (it != functions.end()) {
      return it->second(Args, Context);
   }
   return XPathValue(); // Return empty boolean for unknown functions
}

void XPathFunctionLibrary::register_function(const std::string &Name, XPathFunction Func) {
   functions[Name] = Func;
}

//********************************************************************************************************************
// Core XPath 1.0 Function Implementations

XPathValue XPathFunctionLibrary::function_last(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue((double)Context.size);
}

XPathValue XPathFunctionLibrary::function_position(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue((double)Context.position);
}

XPathValue XPathFunctionLibrary::function_count(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(0.0);
   if (Args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);
   return XPathValue((double)Args[0].node_set.size());
}

XPathValue XPathFunctionLibrary::function_id(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   // TODO: Implement ID function properly
   return XPathValue();
}

XPathValue XPathFunctionLibrary::function_local_name(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   // TODO: Implement local-name function properly
   return XPathValue("");
}

XPathValue XPathFunctionLibrary::function_namespace_uri(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   // TODO: Implement namespace-uri function properly
   return XPathValue("");
}

XPathValue XPathFunctionLibrary::function_name(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   // TODO: Implement name function properly
   return XPathValue("");
}

XPathValue XPathFunctionLibrary::function_string(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         return XPathValue(node_set_value.to_string());
      }
      return XPathValue("");
   }
   return XPathValue(Args[0].to_string());
}

XPathValue XPathFunctionLibrary::function_concat(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   std::string result;
   for (const auto &arg : Args) {
      result += arg.to_string();
   }
   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_starts_with(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 2) return XPathValue(false);
   std::string str = Args[0].to_string();
   std::string prefix = Args[1].to_string();
   return XPathValue(str.substr(0, prefix.length()) IS prefix);
}

XPathValue XPathFunctionLibrary::function_contains(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 2) return XPathValue(false);
   std::string str = Args[0].to_string();
   std::string substr = Args[1].to_string();
   return XPathValue(str.find(substr) != std::string::npos);
}

XPathValue XPathFunctionLibrary::function_substring_before(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   // TODO: Implement substring-before function
   return XPathValue("");
}

XPathValue XPathFunctionLibrary::function_substring_after(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   // TODO: Implement substring-after function
   return XPathValue("");
}

XPathValue XPathFunctionLibrary::function_substring(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() < 2 or Args.size() > 3) return XPathValue("");

   std::string str = Args[0].to_string();
   double start_pos = Args[1].to_number();

   if (std::isnan(start_pos) or std::isinf(start_pos)) return XPathValue("");

   // XPath uses 1-based indexing
   int start_index = (int)std::round(start_pos) - 1;
   if (start_index < 0) start_index = 0;
   if (start_index >= (int)str.length()) return XPathValue("");

   if (Args.size() == 3) {
      double length = Args[2].to_number();
      if (std::isnan(length) or std::isinf(length) or length <= 0) return XPathValue("");
      int len = (int)std::round(length);
      return XPathValue(str.substr(start_index, len));
   }

   return XPathValue(str.substr(start_index));
}

XPathValue XPathFunctionLibrary::function_string_length(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         str = node_set_value.to_string();
      }
   } else {
      str = Args[0].to_string();
   }
   return XPathValue((double)str.length());
}

XPathValue XPathFunctionLibrary::function_normalize_space(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         str = node_set_value.to_string();
      }
   } else {
      str = Args[0].to_string();
   }

   // Remove leading and trailing whitespace, collapse internal whitespace
   size_t start = str.find_first_not_of(" \t\n\r");
   if (start == std::string::npos) return XPathValue("");

   size_t end = str.find_last_not_of(" \t\n\r");
   str = str.substr(start, end - start + 1);

   // Collapse internal whitespace
   std::string result;
   bool in_whitespace = false;
   for (char c : str) {
      if (c == ' ' or c == '\t' or c == '\n' or c == '\r') {
         if (!in_whitespace) {
            result += ' ';
            in_whitespace = true;
         }
      } else {
         result += c;
         in_whitespace = false;
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_translate(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   // TODO: Implement translate function
   return XPathValue("");
}

XPathValue XPathFunctionLibrary::function_boolean(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(false);
   return XPathValue(Args[0].to_boolean());
}

XPathValue XPathFunctionLibrary::function_not(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(true);
   return XPathValue(!Args[0].to_boolean());
}

XPathValue XPathFunctionLibrary::function_true(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue(true);
}

XPathValue XPathFunctionLibrary::function_false(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue(false);
}

XPathValue XPathFunctionLibrary::function_lang(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   // TODO: Implement lang function
   return XPathValue(false);
}

XPathValue XPathFunctionLibrary::function_number(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         return XPathValue(node_set_value.to_number());
      }
      return XPathValue(std::numeric_limits<double>::quiet_NaN());
   }
   return XPathValue(Args[0].to_number());
}

XPathValue XPathFunctionLibrary::function_sum(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(0.0);
   if (Args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);

   double sum = 0.0;
   for (XMLTag *node : Args[0].node_set) {
      if (node) {
         std::vector<XMLTag *> single_node = { node };
         XPathValue node_value(single_node);
         double value = node_value.to_number();
         if (!std::isnan(value)) {
            sum += value;
         }
      }
   }
   return XPathValue(sum);
}

XPathValue XPathFunctionLibrary::function_floor(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::floor(value));
}

XPathValue XPathFunctionLibrary::function_ceiling(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::ceil(value));
}

XPathValue XPathFunctionLibrary::function_round(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::round(value));
}