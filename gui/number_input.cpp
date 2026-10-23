#include "number_input.h"
#include <limits>

ParsedNumber ParseNumber(const std::string& text) {
    ParsedNumber out;
    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    bool sawDigit = false;
    std::uint64_t value = 0;

    for (char c : text) {
        if (c == ' ' || c == ',' || c == '_' || c == '\'') continue;
        if (c < '0' || c > '9') {
            out.error = "digits only";
            return out;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        // Overflow check BEFORE multiplying: value * 10 + digit > kMax.
        if (value > (kMax - digit) / 10) {
            out.error = "too large: must be at most 18446744073709551615";
            return out;
        }
        value = value * 10 + digit;
        sawDigit = true;
    }

    if (!sawDigit) {
        out.error = text.empty() ? "" : "enter a number";
        return out;
    }
    if (value == 0) {
        out.error = "must be at least 1";
        return out;
    }
    out.ok = true;
    out.value = value;
    return out;
}
