#include <bits/stdc++.h>

// TLV Field Encoding — Spec & Task
// Format
// Each field within a message payload is encoded as three parts back to back:
//
// Field ID — 1 byte
// Length — 4 bytes, big-endian (network byte order), giving the length of the value in bytes
// Value — exactly that many bytes, raw
// A payload is just zero or more of these back to back, with no separator and no overall count prefix — you know you've reached the end when you've consumed the whole payload buffer.
//
// Field IDs in use
// 1 — task ID (string)
// 2 — idempotency key (string)
// 3 — status (single byte, one of: 0=pending, 1=running, 2=succeeded, 3=failed)
// 4 — payload (arbitrary bytes — the actual task data or result data)
//
// Can you break this down into simpler tasks to be implemented in C++


enum class TaskStatus : uint8_t {
    Pending = 0,
    Running,
    Succeeded,
    Failed
};

enum class ParseError : uint8_t {
    TruncHeader = 0,
    TruncValue
};

using ResultMap = std::map<uint8_t, std::vector<uint8_t>>;
std::expected<ResultMap, ParseError> reader (const uint8_t* inputstream, const std::size_t size) {
    // TruncHeader Case
    if (size < 5 and size > 0)
            return ParseError::TruncHeader;

    std::vector<message> Messages {};

    if (size == 0)
        return Messages;

    size_t offset {0};

    while (offset < size) {
        // Message Loop

        // Read Type
        const uint8_t type = inputstream[offset++];

        // Read Length
        int32_t length {};
        for (size_t idx {0}; idx < 4; idx++) {
            length <<= 8;
            length |= inputstream[offset++];
        }

        // Read Value
        // Value changes depending on Type.
        std::variant <std::string, uint8_t> value;
        switch (type) {
            case 1:
            case 2 :
            {
                // Read `length` amount of characters into a string.
                if (offset + length > size)
                    return ParseError::TruncValue;

                value.emplace<std::string>(reinterpret_cast<const char*> (&inputstream[offset], length));
                offset += length;
                break;
            }
            case 3: {
                // Read 1 byte of characters into a uint8_t.
                if (offset + 1 > size)
                    return ParseError::TruncValue;

                value = inputstream[offset++];
                break;
            }
            case 4:
            default : std::cout << "Invalid Type"; return ParseError::TruncValue;
        }

        // We have each of the TLV. Now to assemble a `message`, and push it into Messages.



        // This introduces the problem that we are returning errors,
        // and not trying to recover the rest of the valid messages in the payload.

        // Can it be made to keep messages that have the ParseErrors in the them.

        // It adds the headache of not knowing where the corrupted message ends,
        // and where the next valid one begins.
        // This is rectified if the entire payload is rejected and asked to retransmit.

    }
}