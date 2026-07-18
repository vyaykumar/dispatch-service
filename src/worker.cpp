// worker.cc — standalone worker service (Step 2).
//
// Long-running process. Listens for dispatcher connections and handles
// each one on its own std::jthread: receive TASK_SUBMIT -> send TASK_ACK
// -> "execute" -> send TASK_RESULT.
//
// Reuses transport.h/protocol.h from Step 1 as-is — no changes needed
// there. This file is the only thing you're writing.

#include <iostream>
#include <thread>
#include <vector>

#include "protocol.h"
#include "transport.h"

using transport::socket_t;

namespace {

    constexpr uint16_t kPort = 50051;

// Handles exactly one connection, end to end, on its own thread.
//
// TODO: receive a message via protocol::ReceiveMessage(client).
// TODO: check the message type — if it's not kTaskSubmit, decide what to
//       do (log and close? this worker only expects submits for now).
// TODO: send a TASK_ACK back, using the task_id from the submit.
// TODO: "execute" the task — a short sleep is fine as a stand-in for now,
//       the point of this step is the connection/threading shape, not
//       real task execution.
// TODO: build and send a TASK_RESULT (decide what status/payload to use
//       for this stub — kSucceeded with some placeholder payload is fine).
// TODO: close the client socket when done (see transport::CloseSocket).
    void HandleConnection(socket_t client, std::stop_token stopToken) {
        // your code here
    }

// Sets up the listening socket and runs the accept loop.
//
// TODO: create a TCP socket (see main.cc from Step 1's smoke test for the
//       exact socket()/bind()/listen() calls — same pattern applies here).
// TODO: loop calling accept(). For each accepted connection, spawn a
//       std::jthread running HandleConnection with that client socket.
// TODO: decide where those jthreads are kept. A std::jthread that goes out
//       of scope immediately will block the accept loop waiting to join it
//       (that defeats the point of one-thread-per-connection) — you need
//       somewhere to stash them so the accept loop can keep going.
//       (Think about what "somewhere" needs to guarantee about lifetime
//       and thread-safety if the accept loop and a cleanup pass might
//       touch it at different times — you don't need to solve full
//       graceful shutdown yet, just don't paint yourself into a corner.)
// TODO: think about what a clean exit path even looks like here for later
//       — you don't have to build it now, but note it as you go.
    void RunWorker() {
        // your code here
    }

}  // namespace

int main() {
    transport::PlatformInit();

    // TODO: call RunWorker() (or inline its contents here if you'd rather
    // not split it out — your call).

    transport::PlatformCleanup();
    return 0;
}