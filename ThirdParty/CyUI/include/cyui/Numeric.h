#pragma once
#include "cyui/Export.h"
#include <string>
#include <string_view>

namespace cyui {
struct NumericExpressionResult {
    bool valid = false;
    long double value = 0;
    std::string error;
};
// Bounded arithmetic parser, never executes code. Value is the value at edit start.
// + - * / % ^, parentheses, pi, e, k suffix and abs/sqrt/min/max/clamp/round/floor/ceil.
CYUI_API NumericExpressionResult evaluateNumericExpression(std::string_view expression, long double value);
} // namespace cyui
