// protocol.h — task dispatch wire protocol, built on top of transport.h.
//
// Payload encoding is TLV (type-length-value): each field is
// [1-byte field ID][4-byte big-endian length][value bytes], repeated.
// An unknown field ID is simply skipped by ParseFields — this is what
// gives the protocol room to grow without breaking older parsers.

#pragma once

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>
#include <expected>

#include "transport.h"

namespace protocol {

using FieldMap = std::pmr::unordered_map<uint8_t, std::vector<uint8_t>>;

enum class MessageType : uint8_t {
    kTaskSubmit = 1,
    kTaskAck = 2,
    kTaskResult = 3,
    kCancel = 4,
};

enum class TaskStatus : uint8_t {
    kPending = 0,
    kRunning = 1,
    kSucceeded = 2,
    kFailed = 3,
};

enum class FieldId : uint8_t {
    kTaskId = 1,
    kIdempotencyKey = 2,
    kStatus = 3,
    kPayload = 4,
};

enum class ParseError : uint8_t {
    TruncHeader = 0,
    TruncValue
};

// ---- TLV encode/decode ----

class FieldWriter {
public:
    void AddString(FieldId id, const std::string& value) {
        AddBytes(id, std::vector<uint8_t>(value.begin(), value.end()));
    }

    void AddByte(FieldId id, uint8_t value) {
        AddBytes(id, std::vector<uint8_t>{value});
    }

    void AddBytes(FieldId id, const std::vector<uint8_t>& value) {
        buf_.push_back(static_cast<uint8_t>(id));
        uint32_t lenBE = htonl(static_cast<uint32_t>(value.size()));
        uint8_t lenBytes[4];
        std::memcpy(lenBytes, &lenBE, 4);
        buf_.insert(buf_.end(), lenBytes, lenBytes + 4);
        buf_.insert(buf_.end(), value.begin(), value.end());
    }

    const std::vector<uint8_t>& bytes() const { return buf_; }

private:
    std::vector<uint8_t> buf_;
};

/*
    ParseFields function will take in an input stream of bytes,
    and out a raw byte lookup map.

    const uint8_t* input_stream : Pointer to the first byte in the input stream. Marks the start of the payload.
    const std::size_t size      : Size of the payload.

    Return types:
        on success, std::map <uint8_t, std::vector<uint8_t>>.
        on failure, either of enum class ParseError.

    Everything valid is stored as raw bytes in the vector. Can be manipulated as needed.
    NOTE: Last-Write wins, under duplicate contention.
*/

[[nodiscard]] std::expected<FieldMap, ParseError> ParseFields (const uint8_t* input_stream, const std::size_t size) {
    FieldMap output {};

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

inline std::string FieldAsString(const FieldMap& fields, FieldId id) {
    auto it = fields.find(static_cast<uint8_t>(id));
    if (it == fields.end()) return {};
    return std::string(it->second.begin(), it->second.end());
}

// ---- Message structs ----

struct TaskSubmit {
    std::string taskId;
    std::string idempotencyKey;
    std::vector<uint8_t> payload;
};

struct TaskAck {
    std::string taskId;
};

struct TaskResult {
    std::string taskId;
    TaskStatus status;
    std::vector<uint8_t> payload;  // result data, or error info if failed
};

struct Cancel {
    std::string taskId;
};

// ---- Encode: struct -> raw frame, ready for transport::SendFrame ----

inline bool SendTaskSubmit(transport::socket_t s, const TaskSubmit& msg) {
    FieldWriter w;
    w.AddString(FieldId::kTaskId, msg.taskId);
    w.AddString(FieldId::kIdempotencyKey, msg.idempotencyKey);
    w.AddBytes(FieldId::kPayload, msg.payload);
    return transport::SendFrame(s, static_cast<uint8_t>(MessageType::kTaskSubmit), w.bytes());
}

inline bool SendTaskAck(transport::socket_t s, const TaskAck& msg) {
    FieldWriter w;
    w.AddString(FieldId::kTaskId, msg.taskId);
    return transport::SendFrame(s, static_cast<uint8_t>(MessageType::kTaskAck), w.bytes());
}

inline bool SendTaskResult(transport::socket_t s, const TaskResult& msg) {
    FieldWriter w;
    w.AddString(FieldId::kTaskId, msg.taskId);
    w.AddByte(FieldId::kStatus, static_cast<uint8_t>(msg.status));
    w.AddBytes(FieldId::kPayload, msg.payload);
    return transport::SendFrame(s, static_cast<uint8_t>(MessageType::kTaskResult), w.bytes());
}

inline bool SendCancel(transport::socket_t s, const Cancel& msg) {
    FieldWriter w;
    w.AddString(FieldId::kTaskId, msg.taskId);
    return transport::SendFrame(s, static_cast<uint8_t>(MessageType::kCancel), w.bytes());
}

// ---- Decode: raw frame -> typed message ----
// A variant-like manual tag + payload, since we don't want to pull in
// <variant> ceremony for four simple cases.

struct DecodedMessage {
    MessageType type;
    TaskSubmit submit;
    TaskAck ack;
    TaskResult result;
    Cancel cancel;
};

inline std::optional<DecodedMessage> ReceiveMessage(transport::socket_t s) {
    auto raw = transport::RecvFrame(s);
    if (!raw) return std::nullopt;

    auto fields = ParseFields(raw->payload.data(), raw->payload.size());
    if (!fields) return std::nullopt;

    DecodedMessage msg{};
    msg.type = static_cast<MessageType>(raw->type);

    switch (msg.type) {
        case MessageType::kTaskSubmit: {
            msg.submit.taskId = FieldAsString(*fields, FieldId::kTaskId);
            msg.submit.idempotencyKey = FieldAsString(*fields, FieldId::kIdempotencyKey);
            auto it = fields->find(static_cast<uint8_t>(FieldId::kPayload));
            if (it != fields->end()) msg.submit.payload = it->second;
            break;
        }
        case MessageType::kTaskAck: {
            msg.ack.taskId = FieldAsString(*fields, FieldId::kTaskId);
            break;
        }
        case MessageType::kTaskResult: {
            msg.result.taskId = FieldAsString(*fields, FieldId::kTaskId);
            auto statusIt = fields->find(static_cast<uint8_t>(FieldId::kStatus));
            msg.result.status = statusIt != fields->end() && !statusIt->second.empty()
                                     ? static_cast<TaskStatus>(statusIt->second[0])
                                     : TaskStatus::kFailed;
            auto payloadIt = fields->find(static_cast<uint8_t>(FieldId::kPayload));
            if (payloadIt != fields->end()) msg.result.payload = payloadIt->second;
            break;
        }
        case MessageType::kCancel: {
            msg.cancel.taskId = FieldAsString(*fields, FieldId::kTaskId);
            break;
        }
        default:
            return std::nullopt;  // unknown top-level message type
    }
    return msg;
}

}  // namespace protocol
