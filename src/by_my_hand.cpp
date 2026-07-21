// #include <bits/stdc++.h>

#include <cstdint>
#include <vector>
#include <map>
#include <expected>
#include <cstring>
#include <unordered_map>

/*
    Reader function (name pending) will take in an input stream of bytes,
    and out a raw byte lookup map.

    const uint8_t* input_stream : Pointer to the first byte in the input stream. Marks the start of the payload.
    const std::size_t size      : Size of the payload.

    Return types:
        on success, std::map <uint8_t, std::vector<uint8_t>>.
        on failure, either of enum class ParseError.

    Everything valid is stored as raw bytes in the vector. Can be manipulated as needed.
    NOTE: Last-Write wins, under duplicate contention.
*/

enum class ParseError : uint8_t {
    TruncHeader = 0,
    TruncValue
};

using ResultMap = std::pmr::unordered_map<uint8_t, std::vector<uint8_t>>;
[[nodiscard]] std::expected<ResultMap, ParseError> reader (const uint8_t* input_stream, const std::size_t size) {
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
        // for (size_t idx {0}; idx < 4; idx++) {
        //     length <<= 8;
        //     length |= input_stream[offset++];
        // }
        std::memcpy(&length, input_stream + offset, 4);
        if constexpr (std::endian::native == std::endian::little)
            length = std::byteswap(length);
        offset += 4;


        // Since this function don't have to interpret,
        // length-slice of inputs can be pushed into a vector.

        // Check for TruncVal
        if (length > size - offset)
            return std::unexpected(ParseError::TruncValue);

        // std::vector<uint8_t> value;
        // for (size_t idx {0}; idx < length; idx++)
        //     value.emplace_back(input_stream[offset++]);

        std::vector value(input_stream + offset, input_stream + offset + length);
        offset += length;

        // output.insert({type, value});
        // output[type] = std::move(value);
        output.insert_or_assign(type, std::move(value));    // Returns whether it assigned or overwrote.
    }

    return output;
}