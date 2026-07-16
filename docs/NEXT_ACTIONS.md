# Next Actions

## Phase 1.2 — Validate typed control payloads

1. Stay on branch `phase/01-protocol`.
2. Apply the Phase 1.2 control-payload patch.
3. Run `scripts/bootstrap-windows.ps1`.
4. Confirm all three native tests pass:
   - `wiretone_core_tests`
   - `wiretone_protocol_tests`
   - `wiretone_control_payload_tests`
5. Clear Android `.cxx` and `app/build` output.
6. Run `scripts/bootstrap-android.ps1`.
7. Confirm the Android APK and JNI library build with the new protocol sources.
8. Record owner-machine results in `docs/TEST_RESULTS.md`.
9. Commit and push Phase 1.2 as one coherent protocol-contract commit.

## Phase 1.3 — After Phase 1.2 passes

Implement pure C++ logical audio-frame fragmentation and bounded reassembly:

- split one PCM or encoded frame into protocol-sized fragments
- preserve stream, frame, timestamp, and sequence invariants
- accept out-of-order fragments for one bounded in-flight frame window
- reject duplicates and inconsistent fragment metadata
- expire incomplete frames deterministically
- test exact four-fragment behavior for a 3,840-byte PCM frame

Do not add UDP sockets, WASAPI capture, or Android playback until the pure
fragmentation/reassembly tests pass.
