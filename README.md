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

Phases 1 and 2 are complete and owner-validated. Phase 3.1 is tagged and validates the
Pixel 9a AAudio boundary. Phase 3.2 adds:

- a fixed-capacity platform-neutral SPSC PCM playback queue
- exact 960-frame / 20 ms stereo signed 16-bit logical frames
- callback-sized partial reads with FIFO sequence metadata
- drop-newest overflow and exact empty-queue silence policies
- queue, silence-fill, underrun, drop, discard, and sequence counters
- a bounded Android-local native test tone for Pixel validation

The queue remains local to the Android process. Windows PCM handoff, UDP, jitter buffering,
Opus, discovery, pairing, foreground service, and encryption are not part of Phase 3.2.
The first Windows-to-Android PCM path remains Phase 4.

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

The screen opens the AAudio stream, reports negotiated properties, and provides controls for a bounded local test tone, silence output, and stop. Queue and callback counters update while running.

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
