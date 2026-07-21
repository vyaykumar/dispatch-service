#include <bits/stdc++.h>

enum class FieldId : uint8_t {
    kTaskId = 1,
    kIdempotencyKey = 2,
    kStatus = 3,
    kPayload = 4,
};


class message {
public:
    // Constructor, given type and size,
    // that will initialize a uint8_t array, of size `length`.


};

// The following takes only one message inside its payload. What if we want to sent multiple at once?
// A vector of messages(=T+L+V)?
// For now, lets write for a single message payload.

std::vector<uint8_t> write (uint8_t FieldID, uint32_t Length, std::vector<uint8_t> Value) {
    std::vector<uint8_t> payload;

    // Insert FieldID first.
    payload.emplace_back(FieldID);

    // Insert Length in BigEndian format.
    for (size_t idx {0}; idx < 4; idx++) {
        uint8_t element = (Length >> (8*(3-idx))) & 0xFF;
        payload.emplace_back(element);
    }

    // Insert the Value at the end.
    payload.insert(payload.end(), Value.begin(), Value.end());

    // Flimsy checking.
    if (payload.size() != (5+Value.size())) {
        std::cout << "Error occurred. Placeholder.\n";
    }

    return payload;
}