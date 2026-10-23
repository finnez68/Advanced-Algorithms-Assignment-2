// number_input.h -- turns the text in a row's Number box into a uint64_t.
// No ImGui dependency, so the tests can exercise it directly.

#pragma once
#include <cstdint>
#include <string>

struct ParsedNumber {
    bool ok = false;
    std::uint64_t value = 0;
    std::string error; // set when !ok and the text is non-empty
};

// Accepts decimal digits, ignoring spaces, commas, underscores and
// apostrophes used as digit separators ("1,000,000,007"). Rejects empty
// input, 0, anything else non-digit, and values above 2^64 - 1.
ParsedNumber ParseNumber(const std::string& text);
