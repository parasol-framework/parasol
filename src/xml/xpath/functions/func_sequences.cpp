//********************************************************************************************************************
// XPath Sequence Functions

XPathValue XPathFunctionLibrary::function_index_of(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(std::vector<XMLTag *>());

   if ((Args.size() > 2) and Context.expression_unsupported) *Context.expression_unsupported = true;

   const XPathValue &sequence = Args[0];
   const XPathValue &lookup = Args[1];

   size_t length = sequence_length(sequence);
   if (length IS 0) return XPathValue(std::vector<XMLTag *>());

   XPathValue target = extract_sequence_item(lookup, 0);
   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      XPathValue item = extract_sequence_item(sequence, index);
      if (xpath_values_equal(item, target)) {
         builder.nodes.push_back(nullptr);
         builder.attributes.push_back(nullptr);
         builder.strings.push_back(format_xpath_number(double(index + 1)));
      }
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_empty(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(true);

   size_t length = sequence_length(Args[0]);
   return XPathValue(length IS 0);
}

XPathValue XPathFunctionLibrary::function_distinct_values(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());

   if ((Args.size() > 1) and Context.expression_unsupported) *Context.expression_unsupported = true;

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);
   if (length IS 0) return XPathValue(std::vector<XMLTag *>());

   std::unordered_set<std::string> seen;
   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      std::string key = sequence_item_string(sequence, index);
      auto insert_result = seen.insert(key);
      if (insert_result.second) {
         XPathValue item = extract_sequence_item(sequence, index);
         append_value_to_sequence(item, builder);
      }
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_insert_before(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 3) {
      if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
      return Args[0];
   }

   const XPathValue &sequence = Args[0];
   double position_value = Args[1].to_number();
   const XPathValue &insertion = Args[2];

   size_t length = sequence_length(sequence);
   size_t insert_index = 0;

   if (std::isnan(position_value)) insert_index = 0;
   else if (std::isinf(position_value)) insert_index = (position_value > 0.0) ? length : 0;
   else {
      long long floored = (long long)std::floor(position_value);
      if (floored <= 1) insert_index = 0;
      else if (floored > (long long)length) insert_index = length;
      else insert_index = (size_t)(floored - 1);
   }

   if (insert_index > length) insert_index = length;

   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      if (index IS insert_index) append_value_to_sequence(insertion, builder);
      XPathValue item = extract_sequence_item(sequence, index);
      append_value_to_sequence(item, builder);
   }

   if (insert_index >= length) append_value_to_sequence(insertion, builder);

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_remove(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) {
      if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
      return Args[0];
   }

   const XPathValue &sequence = Args[0];
   double position_value = Args[1].to_number();
   size_t length = sequence_length(sequence);

   if (length IS 0) return XPathValue(std::vector<XMLTag *>());
   if (std::isnan(position_value) or std::isinf(position_value)) return sequence;

   long long floored = (long long)std::floor(position_value);
   if (floored < 1) return sequence;
   if (floored > (long long)length) return sequence;

   size_t remove_index = (size_t)(floored - 1);
   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      if (index IS remove_index) continue;
      XPathValue item = extract_sequence_item(sequence, index);
      append_value_to_sequence(item, builder);
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_reverse(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);
   SequenceBuilder builder;

   for (size_t index = length; index > 0; --index) {
      XPathValue item = extract_sequence_item(sequence, index - 1);
      append_value_to_sequence(item, builder);
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_subsequence(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(std::vector<XMLTag *>());

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);
   if (length IS 0) return XPathValue(std::vector<XMLTag *>());

   double start_value = Args[1].to_number();
   if (std::isnan(start_value)) return XPathValue(std::vector<XMLTag *>());

   double min_position = std::ceil(start_value);
   if (std::isnan(min_position)) return XPathValue(std::vector<XMLTag *>());
   if (min_position < 1.0) min_position = 1.0;

   double max_position = std::numeric_limits<double>::infinity();
   if (Args.size() > 2) {
      double length_value = Args[2].to_number();
      if (std::isnan(length_value)) return XPathValue(std::vector<XMLTag *>());
      if (length_value <= 0.0) return XPathValue(std::vector<XMLTag *>());

      max_position = std::ceil(start_value + length_value);
      if (std::isnan(max_position)) return XPathValue(std::vector<XMLTag *>());
   }

   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      double position = double(index + 1);
      if (position < min_position) continue;
      if ((not std::isinf(max_position)) and (position >= max_position)) break;
      XPathValue item = extract_sequence_item(sequence, index);
      append_value_to_sequence(item, builder);
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_unordered(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
   return Args[0];
}

XPathValue XPathFunctionLibrary::function_deep_equal(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(false);

   if ((Args.size() > 2) and Context.expression_unsupported) *Context.expression_unsupported = true;

   const XPathValue &left = Args[0];
   const XPathValue &right = Args[1];

   size_t left_length = sequence_length(left);
   size_t right_length = sequence_length(right);
   if (left_length != right_length) return XPathValue(false);

   for (size_t index = 0; index < left_length; ++index) {
      XPathValue left_item = extract_sequence_item(left, index);
      XPathValue right_item = extract_sequence_item(right, index);
      if (not xpath_values_equal(left_item, right_item)) return XPathValue(false);
   }

   return XPathValue(true);
}

XPathValue XPathFunctionLibrary::function_zero_or_one(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);

   if (length <= 1) return sequence;

   flag_cardinality_error(Context, "zero-or-one", "argument has more than one item");
   return XPathValue();
}

XPathValue XPathFunctionLibrary::function_one_or_more(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);

   if (length IS 0) {
      flag_cardinality_error(Context, "one-or-more", "argument is empty");
      return XPathValue();
   }

   return sequence;
}

XPathValue XPathFunctionLibrary::function_exactly_one(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);

   if (length IS 1) return sequence;

   if (length IS 0) flag_cardinality_error(Context, "exactly-one", "argument is empty");
   else flag_cardinality_error(Context, "exactly-one", "argument has more than one item");

   return XPathValue();
}

