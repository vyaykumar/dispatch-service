// Smoke test: proves raw TCP sockets work end to end on this machine/
// compiler before any real project code gets written. No external
// dependencies — this is the whole point of dropping gRPC.
//
// Implements the Step 1 wire framing from the handout in miniature:
// [4-byte big-endian length][1-byte type][payload bytes]
//
// Runs a trivial echo-ish server on a background thread, then connects
// to it as a client and exchanges one frame.
//
// This is throwaway scaffolding for Step 0 — not part of the real
// dispatcher/worker design.

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
#endif

namespace {

constexpr uint16_t kPort = 50051;

enum class MessageType : uint8_t { kPing = 1, kPong = 2 };

struct Frame {
    MessageType type;
    std::vector<uint8_t> payload;
};

void PlatformInit() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

void PlatformCleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void CloseSocket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

// Reads exactly n bytes into buf, looping over partial reads (TCP makes
// no promise that recv() returns a full message in one call).
bool RecvExact(socket_t s, uint8_t* buf, size_t n) {
    size_t received = 0;
    while (received < n) {
        int chunk = recv(s, reinterpret_cast<char*>(buf + received),
                          static_cast<int>(n - received), 0);
        if (chunk <= 0) return false;
        received += static_cast<size_t>(chunk);
    }
    return true;
}

bool SendFrame(socket_t s, MessageType type, const std::string& payload) {
    uint32_t len = static_cast<uint32_t>(payload.size());
    uint32_t lenBE = htonl(len);
    uint8_t typeByte = static_cast<uint8_t>(type);

    std::vector<uint8_t> buf;
    buf.resize(4 + 1 + payload.size());
    std::memcpy(buf.data(), &lenBE, 4);
    buf[4] = typeByte;
    std::memcpy(buf.data() + 5, payload.data(), payload.size());

    size_t sent = 0;
    while (sent < buf.size()) {
        int chunk = send(s, reinterpret_cast<char*>(buf.data() + sent),
                          static_cast<int>(buf.size() - sent), 0);
        if (chunk <= 0) return false;
        sent += static_cast<size_t>(chunk);
    }
    return true;
}

std::optional<Frame> RecvFrame(socket_t s) {
    uint8_t header[5];
    if (!RecvExact(s, header, 5)) return std::nullopt;

    uint32_t lenBE;
    std::memcpy(&lenBE, header, 4);
    uint32_t len = ntohl(lenBE);
    MessageType type = static_cast<MessageType>(header[4]);

    std::vector<uint8_t> payload(len);
    if (len > 0 && !RecvExact(s, payload.data(), len)) return std::nullopt;

    return Frame{type, std::move(payload)};
}

void RunServer(std::stop_token stopToken) {
    socket_t listener = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(kPort);

    bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    listen(listener, 1);
    std::cout << "[server] listening on port " << kPort << "\n";

    // Accept exactly one connection for this smoke test.
    socket_t client = accept(listener, nullptr, nullptr);
    if (client != kInvalidSocket) {
        auto frame = RecvFrame(client);
        if (frame && frame->type == MessageType::kPing) {
            std::string received(frame->payload.begin(), frame->payload.end());
            std::cout << "[server] received ping: " << received << "\n";
            SendFrame(client, MessageType::kPong, "pong: " + received);
        }
        CloseSocket(client);
    }
    CloseSocket(listener);
}

}  // namespace

int main() {
    PlatformInit();

    std::jthread serverThread(RunServer);

    // Give the server a moment to start listening.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    socket_t client = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(kPort);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(client, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) != 0) {
        std::cout << "[client] connect failed\n";
        return 1;
    }

    SendFrame(client, MessageType::kPing, "hello from client");

    auto reply = RecvFrame(client);
    if (reply && reply->type == MessageType::kPong) {
        std::string message(reply->payload.begin(), reply->payload.end());
        std::cout << "[client] received: " << message << "\n";
        std::cout << "Toolchain OK.\n";
    } else {
        std::cout << "[client] no valid reply received\n";
        CloseSocket(client);
        return 1;
    }

    CloseSocket(client);
    serverThread.join();
    PlatformCleanup();
    return 0;
}
