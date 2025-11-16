#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

bool is_valid_timezone(std::string_view Value);
bool parse_xs_date_components(std::string_view Value, long long &Year, int &Month, int &Day, size_t &NextIndex);
bool is_valid_xs_date(std::string_view Value);
bool is_valid_xs_date_no_timezone(std::string_view Value);
bool is_valid_xs_time(std::string_view Value);
bool is_valid_xs_datetime(std::string_view Value);
std::optional<std::string> extract_date_from_datetime(std::string_view Value);
std::optional<std::string> extract_time_from_datetime(std::string_view Value);
std::optional<std::string> canonicalise_xs_date(std::string_view Value);
std::optional<std::string> canonicalise_xs_time(std::string_view Value);
std::optional<std::string> canonicalise_xs_datetime(std::string_view Value);
