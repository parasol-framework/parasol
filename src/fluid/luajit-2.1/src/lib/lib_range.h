// Range library header for Fluid.
// Copyright (C) 2025 Paul Manias.
//
// Shared definitions for the Range type used by lib_range.cpp and lib_string.cpp

#ifndef LIB_RANGE_H
#define LIB_RANGE_H

#include <cstdint>

//********************************************************************************************************************
// Range structure - stored as userdata payload

struct fluid_range {
   int32_t start;     // Start index (always inclusive)
   int32_t stop;      // End index (exclusive by default)
   int32_t step;      // Step value (default: 1, or -1 for reverse)
   bool inclusive;    // If true, stop is included (default: false)
};

// Metatable name for range userdata
static constexpr const char* RANGE_METATABLE = "Fluid.range";

#endif // LIB_RANGE_H
