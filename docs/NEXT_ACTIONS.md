# Next Actions

## Close Phase 2

1. Commit the Phase 2.4 capture-recovery implementation.
2. Commit the owner-machine validation record.
3. Push branch `phase/02-windows-capture`.
4. Publish annotated tag `phase-2.4-pass`.
5. Fast-forward `main` to the validated Phase 2 checkpoint.
6. Create and push branch `phase/03-android-output`.

## Phase 3.1 — Android native output boundary

Implement the smallest independently testable Android playback increment:

- keep Kotlin limited to lifecycle, permissions/status presentation, and JNI calls
- keep native playback ownership in the Android NDK C++ layer
- open an AAudio output stream in callback mode
- request the locked 48 kHz stereo signed 16-bit PCM contract
- request low-latency performance and shared output without assuming the request is granted
- report the actual negotiated sample rate, channel count, format, sharing mode,
  performance mode, burst size, and buffer capacity
- provide deterministic native start, stop, error, and disconnect states
- render silence from the callback for this initial boundary
- count callback invocations, rendered frames, underruns, and disconnect events
- avoid allocation, locking, logging, JNI calls, and blocking work inside the audio callback

## Phase 3.1 validation gate

- all existing native protocol/capture tests remain green
- new platform-neutral playback-lifecycle tests pass
- Android NDK and debug APK builds pass
- the Pixel 9a opens, starts, and stops the native output stream cleanly
- the app reports negotiated stream properties and non-zero rendered-frame counters
- no UDP receiver, jitter buffer, Opus, discovery, pairing, or encryption is introduced

Do not add network transport or feed captured Windows PCM into Android during Phase 3.1.
The first end-to-end PCM path remains Phase 4.
