#include <bits/stdc++.h>

enum class ParseError : uint8_t {
    TruncHeader = 0,
    TruncValue
};

using ResultMap = std::map<uint8_t, std::vector<uint8_t>>;
std::expected<ResultMap, ParseError> reader (const uint8_t* input_stream, const std::size_t size) {
    // TruncHeader Case
    if (size < 5 and size > 0)
            return std::unexpected(ParseError::TruncHeader);

    ResultMap output {};

    if (size == 0)
        return output;

    size_t offset {0};

    // Don't manipulate value to any type.
    // Hand if off as a uint8_t vector only.
    while (offset < size) {
        // Check for TruncHeader
        if (size - offset < 5)
            return std::unexpected(ParseError::TruncHeader);

        // Read Type
        const uint8_t type = input_stream[offset++];

        // Read Length
        uint32_t length {};
        for (size_t idx {0}; idx < 4; idx++) {
            length <<= 8;
            length |= input_stream[offset++];
        }

        // Since this function don't have to interpret,
        // length-slice of inputs can be pushed into a vector.

        // Check for TruncVal
        if (length > size - offset)
            return std::unexpected(ParseError::TruncValue);

        std::vector<uint8_t> value;
        for (size_t idx {0}; idx < length; idx++)
            value.emplace_back(input_stream[offset++]);

        // output.insert({type, value});
        output[type] = value;
    }

    return output;
}