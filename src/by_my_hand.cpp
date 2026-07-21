#include <bits/stdc++.h>

enum class FieldId : uint8_t {
    kTaskId = 1,
    kIdempotencyKey = 2,
    kStatus = 3,
    kPayload = 4,
};

class FieldWriter {
public:

    void addField (FieldId id, const uint32_t length) {
        payload_.emplace_back(static_cast<uint8_t>(id));
        for (size_t idx {0}; idx < 4; idx++)
            payload_.emplace_back(length >> (8*(3-idx)) & 0xFF);
    }

    void addString(const FieldId id, std::string_view value) {
        addField(id,value.size());

        for (const auto& element : value)
            payload_.emplace_back(static_cast<uint8_t>(element));
    }
    void addByte(const FieldId id, uint8_t value) {
        addField(id,1);

        payload_.emplace_back(value);
    }
    void addRaw(FieldId id, std::span<const uint8_t> value) {
        addField(id,value.size());

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