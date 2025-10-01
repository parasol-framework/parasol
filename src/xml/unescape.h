#pragma once

#include <cstddef>

bool decode_numeric_reference(const char *Start, size_t Length, char *Buffer, size_t &ResultLength);

