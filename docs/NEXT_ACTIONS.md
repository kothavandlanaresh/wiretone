# Next Actions

## Phase 1.4 — Validate in-memory receiver session state

1. Stay on branch `phase/01-protocol`.
2. Apply the Phase 1.4 session-state patch.
3. Run `scripts/bootstrap-windows.ps1`.
4. Confirm all five native tests pass:
   - `wiretone_core_tests`
   - `wiretone_protocol_tests`
   - `wiretone_control_payload_tests`
   - `wiretone_audio_frame_tests`
   - `wiretone_session_tests`
5. Clear Android `.cxx` and `app/build` output.
6. Run `scripts/bootstrap-android.ps1`.
7. Confirm the Android APK and JNI library compile with the session sources.
8. Record owner-machine results, commit, push, and tag Phase 1.4.

## Phase 2 — After Phase 1.4 passes

Begin Windows audio capture with a testable WASAPI loopback boundary:

- enumerate and select the default render endpoint
- open shared-mode loopback capture
- normalize captured samples into the locked 48 kHz stereo PCM contract
- surface silence, discontinuity, device-change, and error events
- test conversion and capture-state logic separately from UDP transport

Do not add UDP transport, Android playback, or Opus during the initial Phase 2
capture increment.
