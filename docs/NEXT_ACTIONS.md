# Next Actions

## Close Phase 3

1. Commit the Phase 3.2 native PCM playback-queue implementation.
2. Commit the owner-machine and Pixel validation record.
3. Push branch `phase/03-android-output`.
4. Publish annotated tag `phase-3.2-pass`.
5. Fast-forward `main` to the validated Phase 3 checkpoint.
6. Create and push branch `phase/04-end-to-end-pcm`.

## Phase 4.1 — First bounded Windows-to-Android PCM path

Implement the smallest independently testable LAN PCM path:

- keep the existing 960-frame / 20 ms stereo PCM16 logical frame as source of truth
- serialize the existing protocol audio-fragment envelope without introducing Opus
- add a bounded Windows UDP sender fed by completed capture frames
- add a bounded Android native UDP receiver feeding the existing PCM playback queue
- preserve sequence and discontinuity metadata end to end
- keep socket ownership and packet parsing in C++
- keep Kotlin limited to lifecycle, endpoint/status display, and explicit start/stop
- expose sent, received, rejected, duplicate, missing-sequence, queue-drop, and rendered counters
- remain LAN-only with a manually configured endpoint for this first path
- do not retain PCM, write files, add cloud services, or add accounts

## Phase 4.1 validation gate

- all existing eleven native tests remain green
- new transport tests cover packet serialization, fragmentation, malformed input,
  sequence gaps, duplicates, bounded queue handoff, and deterministic stop/reset
- Windows MSVC and Android NDK/APK builds pass
- a Pixel 9a receives live Windows loopback PCM over the local network
- the Android queue consumes non-zero remote PCM frames
- sent and received sequence/counter evidence is internally consistent
- no Opus, jitter buffer, discovery, pairing, encryption, foreground service,
  cloud service, account, telemetry, or retained audio is introduced

Do not optimize latency or add codec complexity until the first raw-PCM path is
correct, observable, bounded, and repeatable.
