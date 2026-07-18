// transport.h — raw TCP framing primitives.
//
// This layer knows nothing about tasks, dispatchers, or workers. It only
// solves TCP's "no message boundaries" problem: given a socket, send and
// receive discrete, length-delimited frames reliably, including across
// partial reads/writes.
//
// Frame format: [4-byte big-endian length][1-byte type][payload bytes]
//
// The `type` byte here is a raw wire type — protocol.h interprets it as
// a MessageType and builds real task semantics on top.

#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace transport {

#ifdef _WIN32
using socket_t = SOCKET;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
using socket_t = int;
inline constexpr socket_t kInvalidSocket = -1;
#endif

inline void PlatformInit() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

inline void PlatformCleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

inline void CloseSocket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

struct RawFrame {
    uint8_t type;
    std::vector<uint8_t> payload;
};

// Reads exactly n bytes into buf, looping over partial reads — TCP makes
// no promise that recv() returns a full message in one call.
inline bool RecvExact(socket_t s, uint8_t* buf, size_t n) {
    size_t received = 0;
    while (received < n) {
        int chunk = recv(s, reinterpret_cast<char*>(buf + received),
                          static_cast<int>(n - received), 0);
        if (chunk <= 0) return false;
        received += static_cast<size_t>(chunk);
    }
    return true;
}

inline bool SendFrame(socket_t s, uint8_t type, const std::vector<uint8_t>& payload) {
    uint32_t len = static_cast<uint32_t>(payload.size());
    uint32_t lenBE = htonl(len);

    std::vector<uint8_t> buf;
    buf.resize(4 + 1 + payload.size());
    std::memcpy(buf.data(), &lenBE, 4);
    buf[4] = type;
    if (!payload.empty()) {
        std::memcpy(buf.data() + 5, payload.data(), payload.size());
    }

    size_t sent = 0;
    while (sent < buf.size()) {
        int chunk = send(s, reinterpret_cast<char*>(buf.data() + sent),
                          static_cast<int>(buf.size() - sent), 0);
        if (chunk <= 0) return false;
        sent += static_cast<size_t>(chunk);
    }
    return true;
}

inline std::optional<RawFrame> RecvFrame(socket_t s) {
    uint8_t header[5];
    if (!RecvExact(s, header, 5)) return std::nullopt;

    uint32_t lenBE;
    std::memcpy(&lenBE, header, 4);
    uint32_t len = ntohl(lenBE);
    uint8_t type = header[4];

    std::vector<uint8_t> payload(len);
    if (len > 0 && !RecvExact(s, payload.data(), len)) return std::nullopt;

    return RawFrame{type, std::move(payload)};
}

}  // namespace transport
