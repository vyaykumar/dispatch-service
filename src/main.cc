// Step 1 smoke test: proves protocol.h + transport.h work together end to
// end. Runs a worker on a background thread that speaks the real
// TASK_SUBMIT -> TASK_ACK -> TASK_RESULT sequence, then a dispatcher-ish
// client drives it.
//
// This is still throwaway scaffolding — the real dispatcher/worker
// binaries come in Steps 2-3 and will reuse protocol.h/transport.h as-is.

#include <chrono>
#include <iostream>
#include <thread>

#include "protocol.h"
#include "transport.h"

using transport::socket_t;

namespace {

    constexpr uint16_t kPort = 50051;

    void RunWorker(std::stop_token /*stopToken*/) {
        socket_t listener = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(kPort);

        bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        listen(listener, 1);
        std::cout << "[worker] listening on port " << kPort << "\n";

        socket_t client = accept(listener, nullptr, nullptr);
        if (client != transport::kInvalidSocket) {
            auto msg = protocol::ReceiveMessage(client);
            if (msg && msg->type == protocol::MessageType::kTaskSubmit) {
                std::cout << "[worker] received TASK_SUBMIT: task_id="
                          << msg->submit.taskId
                          << " idempotency_key=" << msg->submit.idempotencyKey << "\n";

                // Ack immediately, before "executing".
                protocol::SendTaskAck(client, {msg->submit.taskId});
                std::cout << "[worker] sent TASK_ACK\n";

                // Pretend to do the work.
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                std::string resultText = "processed: " +
                                         std::string(msg->submit.payload.begin(), msg->submit.payload.end());
                protocol::TaskResult result{
                        msg->submit.taskId,
                        protocol::TaskStatus::kSucceeded,
                        std::vector<uint8_t>(resultText.begin(), resultText.end())
                };
                protocol::SendTaskResult(client, result);
                std::cout << "[worker] sent TASK_RESULT\n";
            }
            transport::CloseSocket(client);
        }
        transport::CloseSocket(listener);
    }

}  // namespace

int main() {
    transport::PlatformInit();

    std::jthread workerThread(RunWorker);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(kPort);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) != 0) {
        std::cout << "[dispatcher] connect failed\n";
        return 1;
    }

    protocol::TaskSubmit submit{
            "task-0001",
            "idem-key-abc",
            std::vector<uint8_t>{'h', 'e', 'l', 'l', 'o'}
    };
    protocol::SendTaskSubmit(sock, submit);
    std::cout << "[dispatcher] sent TASK_SUBMIT\n";

    auto ackMsg = protocol::ReceiveMessage(sock);
    if (!ackMsg || ackMsg->type != protocol::MessageType::kTaskAck) {
        std::cout << "[dispatcher] did not receive expected TASK_ACK\n";
        return 1;
    }
    std::cout << "[dispatcher] received TASK_ACK for task_id=" << ackMsg->ack.taskId << "\n";

    auto resultMsg = protocol::ReceiveMessage(sock);
    if (!resultMsg || resultMsg->type != protocol::MessageType::kTaskResult) {
        std::cout << "[dispatcher] did not receive expected TASK_RESULT\n";
        return 1;
    }
    std::string resultText(resultMsg->result.payload.begin(), resultMsg->result.payload.end());
    std::cout << "[dispatcher] received TASK_RESULT: status="
              << static_cast<int>(resultMsg->result.status)
              << " payload=\"" << resultText << "\"\n";

    std::cout << "Protocol OK.\n";

    transport::CloseSocket(sock);
    workerThread.join();
    transport::PlatformCleanup();
    return 0;
}