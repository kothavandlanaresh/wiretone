# WireTone

WireTone is a private, local-first audio relay for sending Windows 11 system audio to an
Android phone over the same local network.

## Product boundary

WireTone V1 targets one path only:

```text
Windows 11 system audio -> local Wi-Fi -> Pixel 9a speakers
```

No cloud account, telemetry, advertising, or remote relay is part of V1.

## Languages

- C++20: Windows sender, shared protocol/audio engine, Android NDK layer
- Kotlin: thin Android lifecycle and UI layer
- CMake: native builds on Windows and Android

## Current phase

**Phase 1 — Protocol contract**

The repository currently contains:

- a buildable native core library
- a console sender shell
- a shared version-1 packet-envelope library
- typed control-payload encoders, parsers, and strict validation
- allocation-free audio-frame fragmentation and bounded reassembly
- a bounded in-memory receiver session state machine and completed-frame queue
- native exact-byte, round-trip, malformed-input, reassembly, and session-state tests
- a minimal Kotlin Android activity
- a JNI bridge that compiles and loads the shared C++ libraries
- project governance and decision documents

No UDP sockets, audio capture, codec implementation, jitter buffer, or playback code exists yet.

## Build native foundation on Windows

Open **Developer PowerShell for Visual Studio** and run:

```powershell
Set-Location C:\Path\To\wiretone
cmake -S . -B out\build\windows -DWIRETONE_BUILD_TESTS=ON
cmake --build out\build\windows --config Debug
ctest --test-dir out\build\windows -C Debug --output-on-failure
```

Or run:

```powershell
Set-Location C:\Path\To\wiretone
.\scripts\bootstrap-windows.ps1
```

## Build Android shell

Open `apps/android-receiver` in Android Studio. Install the SDK, NDK, CMake, and LLDB
components requested by the project, then run the `app` configuration on the Pixel 9a.

The screen should display the version returned through JNI from the C++ core.

## Documents

- `docs/PROJECT_CHARTER.md`
- `docs/ARCHITECTURE.md`
- `docs/DECISIONS.md`
- `docs/CURRENT_TRUTH.md`
- `docs/NEXT_ACTIONS.md`
- `docs/TEST_RESULTS.md`
- `docs/PROTOCOL.md`

## License

Apache License 2.0. See `LICENSE`.
