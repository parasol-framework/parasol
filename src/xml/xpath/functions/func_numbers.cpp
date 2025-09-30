//********************************************************************************************************************
// XPath Number Functions

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

XPathValue XPathFunctionLibrary::function_sum(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(0.0);
   if (Args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);

   const auto &nodeset = Args[0];
   double sum = 0.0;

   // Handle attribute nodes if present
   if (not nodeset.node_set_attributes.empty()) {
      for (const XMLAttrib *attr : nodeset.node_set_attributes) {
         if (attr) {
            double value = XPathValue::string_to_number(attr->Value);
            if (not std::isnan(value)) {
               sum += value;
            }
         }
      }
   }
   // Handle string values if present
   else if (not nodeset.node_set_string_values.empty()) {
      for (const std::string &str : nodeset.node_set_string_values) {
         double value = XPathValue::string_to_number(str);
         if (not std::isnan(value)) {
            sum += value;
         }
      }
   }
   // Handle regular element nodes
   else {
      for (XMLTag *node : nodeset.node_set) {
         if (node) {
            std::string node_content = XPathValue::node_string_value(node);
            double value = XPathValue::string_to_number(node_content);
            if (not std::isnan(value)) {
               sum += value;
            }
         }
      }
   }
   return XPathValue(sum);
}

XPathValue XPathFunctionLibrary::function_floor(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::floor(value));
}

XPathValue XPathFunctionLibrary::function_ceiling(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::ceil(value));
}

XPathValue XPathFunctionLibrary::function_round(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::round(value));
}

XPathValue XPathFunctionLibrary::function_round_half_to_even(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty() or Args.size() > 2) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);

   int precision = 0;
   if (Args.size() > 1) precision = (int)Args[1].to_number();

   double scaled = value;
   double factor = 1.0;
   bool negative_precision = false;

   if (precision > 0) {
      factor = std::pow(10.0, precision);
      if (std::isnan(factor) or std::isinf(factor) or factor IS 0.0) return XPathValue(value);
      scaled = value * factor;
      if (std::isnan(scaled) or std::isinf(scaled)) return XPathValue(value);
   }
   else if (precision < 0) {
      negative_precision = true;
      factor = std::pow(10.0, -precision);
      if (std::isnan(factor) or std::isinf(factor) or factor IS 0.0) return XPathValue(value);
      scaled = value / factor;
   }

   double rounded_scaled = std::nearbyint(scaled);

   if (std::isnan(rounded_scaled) or std::isinf(rounded_scaled)) return XPathValue(rounded_scaled);

   double result = rounded_scaled;
   if (precision > 0) {
      result = rounded_scaled / factor;
   }
   else if (negative_precision) {
      result = rounded_scaled * factor;
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_abs(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);

   return XPathValue(std::fabs(value));
}

XPathValue XPathFunctionLibrary::function_min(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double minimum = numbers[0];
   for (size_t index = 1; index < numbers.size(); ++index) {
      if (numbers[index] < minimum) minimum = numbers[index];
   }

   return XPathValue(minimum);
}

XPathValue XPathFunctionLibrary::function_max(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double maximum = numbers[0];
   for (size_t index = 1; index < numbers.size(); ++index) {
      if (numbers[index] > maximum) maximum = numbers[index];
   }

   return XPathValue(maximum);
}

XPathValue XPathFunctionLibrary::function_avg(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double total = 0.0;
   for (double value : numbers) total += value;

   return XPathValue(total / (double)numbers.size());
}

