#include <bits/stdc++.h>

enum class FieldId : uint8_t {
    kTaskId = 1,
    kIdempotencyKey = 2,
    kStatus = 3,
    kPayload = 4,
};

class FieldWriter {
public:
    void addString(FieldId id, std::string_view value) {
        payload_.emplace_back(static_cast<uint8_t>(id));

        const uint32_t length = value.size();
        for (size_t idx {0}; idx < 4; idx++)
            payload_.emplace_back(length >> (8*(3-idx)) & 0xFF);

        for (const auto& element : value)
            payload_.emplace_back(static_cast<uint8_t>(element));
    }
    void addByte(FieldId id, uint8_t value) {
        payload_.emplace_back(static_cast<uint8_t>(id));

        constexpr uint32_t length = 1;
        for (size_t idx {0}; idx < 4; idx++)
            payload_.emplace_back(length >> (8*(3-idx)) & 0xFF);

        payload_.emplace_back(value);
    }
    void addRaw(FieldId id, std::span<const uint8_t> value) {
        payload_.emplace_back(static_cast<uint8_t>(id));

        const uint32_t length = value.size();
        for (size_t idx {0}; idx < 4; idx++)
            payload_.emplace_back(length >> (8*(3-idx)) & 0xFF);

        payload_.insert(payload_.end(), value.begin(), value.end());
    }

    // Inspection copy.
    [[nodiscard]] const std::vector<uint8_t>& payload() const {
        return payload_;
    }

    // Consumption copy. Do not use FieldWriter after usage.
    [[nodiscard]] std::vector<uint8_t> finish() && {
        return std::move(payload_);
    }

private:
    std::vector<uint8_t> payload_;
};





// < --------------------------- First Pass Begins Here ---------------------------->
//
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