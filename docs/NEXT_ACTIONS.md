# Next Actions

## Phase 1.3 — Validate audio fragmentation and bounded reassembly

1. Stay on branch `phase/01-protocol`.
2. Apply the Phase 1.3 audio-frame patch.
3. Run `scripts/bootstrap-windows.ps1`.
4. Confirm all four native tests pass:
   - `wiretone_core_tests`
   - `wiretone_protocol_tests`
   - `wiretone_control_payload_tests`
   - `wiretone_audio_frame_tests`
5. Clear Android `.cxx` and `app/build` output.
6. Run `scripts/bootstrap-android.ps1`.
7. Confirm the Android APK and JNI library build with the new protocol sources.
8. Commit, push, and tag Phase 1.3.

## Phase 1.4 — After Phase 1.3 passes

Implement an in-memory sender/receiver session state machine around the locked
packet and payload types:

- require `stream_start` before audio
- reject audio for the wrong stream or after `stream_stop`
- track heartbeat liveness without sockets
- feed completed audio frames from the reassembler into a bounded receiver queue
- expose deterministic counters for malformed, duplicate, expired, and dropped data
- test restart, discontinuity, stop, and stream-ID replacement behavior

Do not add UDP sockets, WASAPI capture, Android playback, or Opus until the
pure session-state tests pass.
