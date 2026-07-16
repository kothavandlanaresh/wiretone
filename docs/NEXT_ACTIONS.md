# Next Actions

## Close Phase 3.1

1. Commit the Android AAudio output-boundary implementation.
2. Commit the owner-machine and Pixel 9a validation record.
3. Push branch `phase/03-android-output`.
4. Publish annotated tag `phase-3.1-pass`.

## Phase 3.2 — Bounded native PCM playback queue

Stay on branch `phase/03-android-output`.

Implement the next independently testable Android playback increment:

- add a fixed-capacity single-producer/single-consumer PCM queue in platform-neutral C++
- accept exact 960-frame / 20 ms stereo signed 16-bit PCM logical frames
- preserve frame sequence and discontinuity metadata at the queue boundary
- consume queued PCM from the AAudio callback without allocation, locking, JNI,
  logging, sleeping, or blocking work
- render silence when the queue is empty and count underrun callbacks and frames
- define an explicit bounded overflow policy and expose dropped-frame counters
- reset queued and partially consumed data deterministically on stop or disconnect
- provide a native local test producer that submits a deterministic, amplitude-limited
  PCM signal for Pixel playback validation
- keep Kotlin limited to invoking the local test and displaying native counters
- keep all PCM local to the Android process

## Phase 3.2 validation gate

- all existing ten native tests remain green
- new queue tests cover FIFO order, capacity, overflow policy, partial callback reads,
  empty-queue silence, reset, discontinuity, and sequence metadata
- Android NDK and debug APK builds pass
- the Pixel 9a plays the bounded local native test signal and returns cleanly to ready
- queued, consumed, silent-fill, overflow, and underrun counters are visible
- the callback remains real-time safe
- no UDP, Windows PCM handoff, jitter buffer, Opus, discovery, pairing, encryption,
  or foreground service is introduced

The first Windows-to-Android PCM path remains Phase 4.
