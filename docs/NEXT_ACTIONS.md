# Next Actions

## Phase 2.1 — Windows capture boundary

Create the next workstream branch from the validated Phase 1 checkpoint:

```powershell
Set-Location "$HOME\source\wiretone"
git switch -c "phase/02-windows-capture"
git push -u origin "phase/02-windows-capture"
```

Implement a testable Windows WASAPI loopback capture boundary:

- enumerate the default render endpoint
- open shared-mode loopback capture
- keep COM and WASAPI ownership inside the Windows sender layer
- normalize capture output into the locked 48 kHz stereo PCM contract
- surface silence, discontinuity, device-change, and recoverable-error events
- separate sample conversion tests from live device integration tests
- preserve the protocol library as a platform-independent dependency

## Phase 2.1 validation gate

- native unit tests for sample-format normalization and capture-state transitions
- Windows integration check against the current default render endpoint
- sender shell can start capture, report format/device details, and stop cleanly
- no protocol regression across the existing five native test executables

Do not add UDP transport, Android playback, jitter-buffer timing, or Opus during
the initial Phase 2 capture increment.
