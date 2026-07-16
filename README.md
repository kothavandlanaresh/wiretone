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

**Phase 3 — Android output in progress**

Phases 1 and 2 are complete and owner-validated. The repository now contains:

- the versioned packet/control protocol and bounded receiver session
- Windows WASAPI loopback capture, PCM normalization, exact 20 ms framing, and recovery
- a platform-neutral playback lifecycle
- an Android NDK AAudio output owner in callback mode
- requested 48 kHz stereo signed 16-bit PCM, shared output, and low-latency performance
- negotiated stream-property and callback/underrun/disconnect counters
- a thin Kotlin UI and JNI surface for open, start, stop, close, and status

Phase 3.1 renders silence only. It proves native Android output negotiation and lifecycle
without introducing network transport, PCM queues, jitter buffering, Opus, discovery,
pairing, or encryption. The first Windows-to-Android PCM path remains Phase 4.

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

## Build Android output boundary

Open `apps/android-receiver` in Android Studio. Install the SDK, NDK, CMake, and LLDB
components requested by the project, then run the `app` configuration on the Pixel 9a.

The screen opens the AAudio stream, reports negotiated properties, and provides controls to start and stop silence rendering. Callback and rendered-frame counters should become non-zero while running.

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
