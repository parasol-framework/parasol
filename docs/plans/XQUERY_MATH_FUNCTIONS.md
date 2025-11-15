# XQuery 3.0 Math Functions Implementation Plan

## Overview

This plan outlines the implementation of XQuery 3.0 math functions to bring the Parasol XQuery module into full compliance with the W3C XQuery 3.0 specification. The math namespace (`http://www.w3.org/2005/xpath-functions/math`) was introduced in XQuery 3.0 and provides 16 mathematical functions for advanced numerical operations.

## Current State Analysis

### Existing Number Functions

The XQuery module currently implements basic numeric functions in `src/xquery/functions/func_numbers.cpp`:

**Implemented (XPath 1.0/2.0):**
- `number()` - Convert to number
- `sum()` - Sum of sequence
- `floor()` - Round down
- `ceiling()` - Round up
- `round()` - Round to nearest
- `round-half-to-even()` - Banker's rounding (XPath 2.0)
- `abs()` - Absolute value (XPath 2.0)
- `min()` - Minimum value (XPath 2.0)
- `max()` - Maximum value (XPath 2.0)
- `avg()` - Average value (XPath 2.0)

### Missing XQuery 3.0 Math Functions

The following 16 functions from the `math:` namespace are **not yet implemented**:

**Trigonometric Functions (7 functions):**
1. `math:acos(xs:double?) → xs:double?` - Arc cosine
2. `math:asin(xs:double?) → xs:double?` - Arc sine
3. `math:atan(xs:double?) → xs:double?` - Arc tangent
4. `math:atan2(xs:double, xs:double) → xs:double` - Two-argument arc tangent
5. `math:cos(xs:double?) → xs:double?` - Cosine
6. `math:sin(xs:double?) → xs:double?` - Sine
7. `math:tan(xs:double?) → xs:double?` - Tangent

**Exponential & Logarithmic Functions (5 functions):**
8. `math:exp(xs:double?) → xs:double?` - Natural exponential (e^x)
9. `math:exp10(xs:double?) → xs:double?` - Base-10 exponential (10^x)
10. `math:log(xs:double?) → xs:double?` - Natural logarithm
11. `math:log10(xs:double?) → xs:double?` - Base-10 logarithm
12. `math:pow(xs:double?, numeric) → xs:double?` - Power function

**Other Functions (2 functions):**
13. `math:sqrt(xs:double?) → xs:double?` - Square root
14. `math:pi() → xs:double` - Pi constant (π ≈ 3.14159...)

## Implementation Strategy

### Phase 1: Code Structure Setup

**1.1 Create new source file**

Create `src/xquery/functions/func_math.cpp` for all math namespace functions.

**File structure:**
```cpp
// XQuery 3.0 Math Namespace Functions
// Namespace: http://www.w3.org/2005/xpath-functions/math

#include <cmath>
#include <limits>
#include "../api/xquery_functions.h"

// Constants
static constexpr double M_PI_VALUE = 3.14159265358979323846;

// Trigonometric functions
XPathVal XPathFunctionLibrary::function_math_pi(...)
XPathVal XPathFunctionLibrary::function_math_sin(...)
XPathVal XPathFunctionLibrary::function_math_cos(...)
// ... etc
```

**1.2 Update CMakeLists.txt**

Add the new source file to the xquery_lib object library in `src/xquery/CMakeLists.txt`:

```cmake
add_library (xquery_lib OBJECT
   # ... existing files ...
   "${CMAKE_CURRENT_SOURCE_DIR}/functions/func_math.cpp")
```

### Phase 2: Function Implementation

**2.1 Namespace Support**

The functions must be callable with the `math:` prefix. The existing function library already supports namespace-aware function calls through the parser, so functions should be registered with the full namespace-qualified name:

- Register as: `"math:sin"`, `"math:cos"`, etc.
- The parser in `parse/xquery_parser.cpp` handles namespace resolution

**2.2 Implementation Guidelines**

Each function must follow these patterns:

**Empty Sequence Handling:**
```cpp
// For xs:double? parameter (optional)
if (Args.empty() or Args[0].Type == XPVT::NodeSet and Args[0].node_set.empty()) {
   return XPathVal(); // Return empty sequence
}
```

**NaN and Infinity Handling:**
```cpp
double value = Args[0].to_number();
if (std::isnan(value)) return XPathVal(std::numeric_limits<double>::quiet_NaN());
if (std::isinf(value)) return XPathVal(value); // Preserve +/-infinity
```

**Standard Math Library Mapping:**
- Use `std::sin()`, `std::cos()`, `std::tan()`
- Use `std::asin()`, `std::acos()`, `std::atan()`, `std::atan2()`
- Use `std::exp()`, `std::log()`, `std::log10()`
- Use `std::pow()`, `std::sqrt()`

### Phase 3: Function Registration

**3.1 Update function_library.cpp**

Add function declarations to `src/xquery/api/xquery_functions.h`:

```cpp
// Math namespace functions (XQ3.0)
static XPathVal function_math_pi(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_sin(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_cos(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_tan(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_asin(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_acos(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_atan(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_atan2(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_sqrt(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_exp(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_exp10(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_log(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_log10(const std::vector<XPathVal> &Args, const XPathContext &Context);
static XPathVal function_math_pow(const std::vector<XPathVal> &Args, const XPathContext &Context);
```

**3.2 Register functions in constructor**

In `src/xquery/functions/function_library.cpp`, add to the `XPathFunctionLibrary::XPathFunctionLibrary()` constructor:

```cpp
// Math Namespace Functions (XQ3.0)
register_function("math:pi", function_math_pi);
register_function("math:sin", function_math_sin);
register_function("math:cos", function_math_cos);
register_function("math:tan", function_math_tan);
register_function("math:asin", function_math_asin);
register_function("math:acos", function_math_acos);
register_function("math:atan", function_math_atan);
register_function("math:atan2", function_math_atan2);
register_function("math:sqrt", function_math_sqrt);
register_function("math:exp", function_math_exp);
register_function("math:exp10", function_math_exp10);
register_function("math:log", function_math_log);
register_function("math:log10", function_math_log10);
register_function("math:pow", function_math_pow);
```

### Phase 4: Testing

**4.1 Unit Tests**

Create comprehensive unit tests in `src/xquery/tests/test_math.fluid`:

```fluid
-- XQuery 3.0 Math Functions Tests

include 'xml'

-- Test basic trig functions
function test_sin()
   local xml = obj.new('xml', {
      statement = '<?xml version="1.0"?><root xmlns:math="http://www.w3.org/2005/xpath-functions/math"/>'
   })

   local err, result = xml.mtEvaluate('math:sin(0)')
   assert(err == ERR_Okay, 'sin(0) failed')
   assert(math.abs(tonumber(result) - 0) < 0.0001, 'sin(0) should be ~0')
end

-- Test pi constant
function test_pi()
   local xml = obj.new('xml', {
      statement = '<?xml version="1.0"?><root xmlns:math="http://www.w3.org/2005/xpath-functions/math"/>'
   })

   local err, result = xml.mtEvaluate('math:pi()')
   assert(err == ERR_Okay, 'pi() failed')
   assert(math.abs(tonumber(result) - 3.14159265358979) < 0.0001, 'pi() incorrect')
end

-- Test empty sequence handling
function test_empty_sequence()
   local xml = obj.new('xml', {
      statement = '<?xml version="1.0"?><root xmlns:math="http://www.w3.org/2005/xpath-functions/math"/>'
   })

   local err, result = xml.mtEvaluate('math:sin(())')
   assert(err == ERR_Okay, 'sin(()) failed')
   assert(result == '', 'sin(()) should return empty sequence')
end

-- ... additional tests for all functions

return {
   -- Initialisation section for Flute
}
```

**4.2 W3C QT3 Test Suite**

The existing `test_qt_math.fluid` already has infrastructure to run W3C XQuery Test Suite tests from the `QT3_1_0/math/` folder. These tests will automatically validate all math functions once implemented.

**4.3 Edge Case Testing**

Test cases must cover:
- Empty sequences: `math:sin(())`
- NaN inputs: `math:sin(number('NaN'))`
- Infinity: `math:sin(1 div 0)`
- Negative infinity: `math:sin(-1 div 0)`
- Out of domain: `math:asin(2)` → NaN
- Special values: `math:sin(math:pi())` → ~0

**4.4 Register test in CMakeLists.txt**

Add to `src/xquery/CMakeLists.txt`:

```cmake
flute_test (${MOD}_math "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_math.fluid")
```

### Phase 5: Special Considerations

**5.1 Precision Requirements**

The W3C specification requires IEEE 754 double-precision floating-point arithmetic:
- Use `double` type throughout
- Handle subnormal numbers correctly
- Preserve signed zero (-0.0 vs +0.0)
- Return NaN for domain errors (e.g., `asin(2)`)

**5.2 Platform Portability**

The C++ standard library math functions are portable, but verify:
- Windows (MSVC and MinGW)
- Linux (GCC/Clang)
- Consistent NaN handling across platforms

**5.3 Performance Optimization**

Consider:
- Inline simple functions (pi constant)
- The existing unity build system will batch compile
- No need for lookup tables (modern CPUs have fast trig)

## Implementation Checklist

### Code Changes
- [ ] Create `src/xquery/functions/func_math.cpp`
- [ ] Add function declarations to `src/xquery/api/xquery_functions.h`
- [ ] Implement all 14 math functions in `func_math.cpp`
- [ ] Register functions in `function_library.cpp` constructor
- [ ] Update `CMakeLists.txt` to include new source file

### Functions to Implement
**Trigonometric:**
- [ ] `math:pi()` - Pi constant
- [ ] `math:sin()` - Sine
- [ ] `math:cos()` - Cosine
- [ ] `math:tan()` - Tangent
- [ ] `math:asin()` - Arc sine
- [ ] `math:acos()` - Arc cosine
- [ ] `math:atan()` - Arc tangent
- [ ] `math:atan2()` - Two-argument arc tangent

**Exponential/Logarithmic:**
- [ ] `math:exp()` - Natural exponential
- [ ] `math:exp10()` - Base-10 exponential
- [ ] `math:log()` - Natural logarithm
- [ ] `math:log10()` - Base-10 logarithm
- [ ] `math:pow()` - Power function

**Other:**
- [ ] `math:sqrt()` - Square root

### Testing
- [ ] Create `src/xquery/tests/test_math.fluid`
- [ ] Write unit tests for all 14 functions
- [ ] Test empty sequence handling
- [ ] Test NaN and infinity edge cases
- [ ] Test domain errors (out-of-range inputs)
- [ ] Register test in `CMakeLists.txt`
- [ ] Verify W3C QT3 tests pass (if available)

### Validation
- [ ] Build compiles successfully
- [ ] All unit tests pass
- [ ] W3C conformance tests pass
- [ ] No trailing whitespace in code
- [ ] Code follows project style (3-space indents, `and`/`or` keywords, `IS` macro)
- [ ] Documentation comments added
- [ ] Cross-platform testing (Windows/Linux)

## References

- **W3C XQuery 3.0 Specification:** https://www.w3.org/TR/xquery-30/
- **XPath Functions and Operators 3.0:** https://www.w3.org/TR/xpath-functions-30/
- **Math Namespace Documentation:** https://www.w3.org/XML/Group/qtspecs/specifications/xpath-functions-30/html/ns-xpath-math-functions.xhtml
- **W3C XQuery Test Suite (QT3):** Used in `test_qt_math.fluid` for comprehensive validation

## Estimated Effort

- **Implementation:** 4-6 hours
  - Function implementation: 2-3 hours
  - Integration and registration: 1 hour
  - Initial testing: 1-2 hours
- **Testing:** 2-3 hours
  - Comprehensive unit tests: 1-2 hours
  - W3C QT3 test debugging: 1 hour
- **Total:** 6-9 hours

## Success Criteria

1. All 14 math namespace functions implemented and working
2. Full compliance with W3C XQuery 3.0 math namespace specification
3. All unit tests passing
4. W3C QT3 conformance tests passing (when available)
5. Code follows project standards (no `&&`/`||`/`==`/`static_cast`)
6. Functions work correctly with namespace prefix `math:`
7. Proper handling of empty sequences, NaN, and infinity
8. Cross-platform compatibility verified

## Notes

- The existing XQuery module already has strong foundation with 80+ functions implemented
- The math namespace functions are straightforward wrappers around C++ standard library
- The main complexity is ensuring W3C specification compliance for edge cases
- The test infrastructure (`test_qt_math.fluid`) is already in place
- This implementation completes XQuery 3.0 numeric function support
