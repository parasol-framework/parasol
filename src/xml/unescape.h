#pragma once

#include <cstddef>
#include "xml.h"

bool decode_numeric_reference(const char *Start, size_t Length, char *Buffer, size_t &ResultLength);
void unescape_all(extXML *Self, TAGS &Tags);
