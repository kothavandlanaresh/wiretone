# Next Actions

## Phase 1.1 — Validate the locked packet envelope

1. Create or switch to branch `phase/01-protocol`.
2. Commit the completed Phase 0 checkpoint before applying Phase 1 files.
3. Apply the Phase 1.1 protocol patch.
4. Run `scripts/bootstrap-windows.ps1`.
5. Confirm both `wiretone_core_tests` and `wiretone_protocol_tests` pass.
6. Run `scripts/bootstrap-android.ps1`.
7. Confirm the Android APK and JNI library still build with the protocol library.
8. Record the owner-machine results in `docs/TEST_RESULTS.md`.
9. Commit Phase 1.1 as one coherent protocol-contract commit.

## Phase 1.2 — After Phase 1.1 passes

Implement typed serializers and parsers for:

- `stream_start`
- `stream_stop`
- `heartbeat`
- `receiver_report`
- protocol error payloads

Do not add UDP sockets or audio capture until the typed control-payload tests pass.
