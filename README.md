# Step 0 — Toolchain Smoke Test (Sockets Edition)

No external dependencies. No vcpkg, no WSL, no separate compiler install.
This should build with whatever CLion already has configured.

## Build

Just open this folder in CLion — it'll detect `CMakeLists.txt` and offer to
build the `smoke_test` target. Or from the command line:

```powershell
cmake -B build
cmake --build build
```

Then run it:

```powershell
.\build\smoke_test.exe
```

Expected output:

```
[server] listening on port 50051
[server] received ping: hello from client
[client] received: pong: hello from client
Toolchain OK.
```

If you see that, Step 0 is done. This also already implements the Step 1
wire framing in miniature — `[4-byte length][1-byte type][payload]` — so
`main.cc` doubles as a working reference for the framing format before you
build the real `TASK_SUBMIT`/`TASK_RESULT`/`CANCEL` protocol on top of it.

## Notes

- On Windows this links `ws2_32` (Winsock) — handled automatically by
  `CMakeLists.txt`, nothing to install separately.
- The code is written to be portable (works on Linux/macOS too, via the
  `#ifdef _WIN32` branches), in case you ever do want to build/test this on
  the homelab or elsewhere later.
