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

**Phase 2 — Windows capture in progress**

Phase 1 is complete and owner-validated. The repository now contains:

- a shared version-1 packet and control-payload contract
- bounded audio fragmentation, reassembly, and receiver-session state
- a platform-neutral capture lifecycle and captured-packet normalization contract
- a Windows WASAPI loopback owner for the default render endpoint
- shared-mode packet draining with frame, flag, and timestamp counters
- deterministic 48 kHz stereo float-to-signed-16-bit PCM conversion
- native protocol, lifecycle, and captured-packet tests
- a minimal Kotlin Android activity and JNI bridge

Phase 2.2 drains and normalizes supported capture packets into local scratch memory.
It does not yet assemble continuous 20 ms logical PCM frames or recover from default
device changes. UDP sockets, Opus, Android playback, jitter buffering, discovery,
pairing, and encryption also remain unimplemented.

## Build and test on Windows

```powershell
Set-Location C:\Path\To\wiretone
.\scripts\bootstrap-windows.ps1
```

The bootstrap performs a clean native rebuild and runs all tests.

To execute the live WASAPI endpoint probe:

```powershell
Set-Location C:\Path\To\wiretone
.\scripts\run-sender-shell.ps1
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
