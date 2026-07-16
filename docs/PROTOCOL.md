# WireTone Protocol

**Protocol version:** 1
**Status:** Phase 1.2 packet envelope and control payloads locked
**Transport:** UDP on the local network
**Byte order:** Network byte order (big-endian) for every multi-byte integer in
WireTone headers and control payloads

This document is the source of truth for packets exchanged between the Windows
sender and Android receiver. Implementations must reject malformed packets rather
than guessing what the sender intended.

## 1. Initial audio contract

The first end-to-end stream will use these defaults:

- sample rate: 48,000 Hz
- channels: 2, interleaved stereo
- logical audio frame duration: 20 ms
- samples per channel per logical frame: 960
- temporary PCM codec: signed 16-bit little-endian (`pcm_s16le`)
- release codec target: Opus
- initial Opus target bitrate: 128–160 kbit/s
- initial receiver buffering target: 60–100 ms

The packet envelope supports fragmenting one logical audio frame across multiple
UDP datagrams. This is required for the temporary PCM phase because one 20 ms
stereo PCM frame is 3,840 bytes. A typical Opus frame should fit in one datagram.

## 2. Datagram limits

- maximum UDP datagram emitted by WireTone: 1,200 bytes
- fixed WireTone header: 32 bytes
- maximum payload per datagram: 1,168 bytes

The 1,200-byte ceiling is deliberately below common Ethernet/Wi-Fi path MTUs so
WireTone does not depend on IP fragmentation.

## 3. Fixed packet header

Every datagram begins with this exact 32-byte header.

| Offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 4 | `magic` | ASCII `WTPK` (`57 54 50 4B`) |
| 4 | 1 | `version` | Protocol version; currently `1` |
| 5 | 1 | `packet_type` | Packet type listed below |
| 6 | 2 | `flags` | Packet flags, big-endian |
| 8 | 4 | `stream_id` | Random non-zero identifier for one stream |
| 12 | 4 | `sequence_number` | Increments once per UDP datagram |
| 16 | 8 | `timestamp_samples` | Logical playback position in sample frames |
| 24 | 2 | `payload_size` | Bytes following the fixed header |
| 26 | 1 | `fragment_index` | Zero-based fragment index |
| 27 | 1 | `fragment_count` | Total fragments; must be at least one |
| 28 | 4 | `frame_id` | Logical audio-frame identifier; zero for control packets |

### 3.1 Identifiers and wrapping

- `stream_id` must never be zero. A new sender session chooses a fresh random
  non-zero value.
- `sequence_number` increments for every datagram, including control datagrams,
  and wraps modulo `2^32`.
- `frame_id` starts at one for audio and increments for each logical audio frame.
  Zero is reserved for control packets.
- All fragments belonging to one audio frame use the same `stream_id`,
  `timestamp_samples`, and `frame_id`.
- Each fragment still receives its own `sequence_number`.

### 3.2 Timestamp meaning

`timestamp_samples` counts sample frames at the stream sample rate, not bytes and
not wall-clock nanoseconds. For the initial 48 kHz / 20 ms contract, consecutive
logical audio frames normally differ by 960.

Control packets may use zero unless their payload definition says otherwise.

## 4. Packet types

| Value | Name | Payload size | Purpose |
|---:|---|---:|---|
| 1 | `stream_start` | 16 bytes | Announces codec and audio format |
| 2 | `stream_stop` | 1 byte | Ends the current stream with a reason code |
| 3 | `audio` | 0–1,168 bytes | One complete or fragmented audio frame |
| 4 | `heartbeat` | 8 bytes | Keeps liveness and sender timing visible |
| 5 | `receiver_report` | 20 bytes | Reports loss, jitter, buffering, and underruns |
| 6 | `error` | 2–258 bytes | Reports a protocol/session error |

Control packets (`stream_start`, `stream_stop`, `heartbeat`, `receiver_report`,
and `error`) must have:

- `fragment_index = 0`
- `fragment_count = 1`
- `frame_id = 0`
- `flags = 0`

Unknown packet types are rejected in protocol version 1.

## 5. Flags

Flags are valid only on `audio` packets.

| Bit | Value | Name | Meaning |
|---:|---:|---|---|
| 0 | `0x0001` | `discontinuity` | First frame after a capture reset, device change, or known gap |
| 1 | `0x0002` | `silence` | Payload may be empty and receiver should synthesize silence |

Every other bit is reserved and must be zero. Unknown flag bits are rejected.

An audio packet with an empty payload is valid only when `silence` is set.

## 6. Control payloads

### 6.1 `stream_start` — 16 bytes

| Offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 1 | `codec` | `1 = pcm_s16le`, `2 = opus` |
| 1 | 1 | `channels` | Initial value `2` |
| 2 | 2 | `frame_duration_ms` | Initial value `20` |
| 4 | 4 | `sample_rate` | Initial value `48000` |
| 8 | 4 | `target_bitrate` | `0` for PCM; target bits/s for Opus |
| 12 | 2 | `pre_skip_samples` | `0` for PCM; codec delay for Opus if needed |
| 14 | 2 | `reserved` | Must be zero |

A receiver must reject unsupported codec, sample rate, channel count, or frame
size instead of silently interpreting the stream differently.

Phase 1.2 locks these semantic rules:

- `codec` must be `pcm_s16le` or `opus`
- `channels` must be `2`
- `frame_duration_ms` must be `20`
- `sample_rate` must be `48000`
- PCM requires `target_bitrate = 0` and `pre_skip_samples = 0`
- Opus requires a non-zero `target_bitrate`
- `reserved` must be zero

### 6.2 `stream_stop` — 1 byte

Initial reason values:

- `0`: normal stop
- `1`: sender shutdown
- `2`: capture device changed
- `3`: unrecoverable sender error

Unknown reason values may be displayed as an unspecified stop reason.

### 6.3 `heartbeat` — 8 bytes

Unsigned sender monotonic time in microseconds, big-endian. This is diagnostic
and liveness data; it is not the audio playback timestamp.

### 6.4 `receiver_report` — 20 bytes

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | highest sequence number received |
| 4 | 4 | cumulative missing datagrams |
| 8 | 4 | cumulative late datagrams |
| 12 | 4 | estimated network jitter in microseconds |
| 16 | 2 | currently buffered audio in milliseconds |
| 18 | 2 | cumulative playback underruns |

### 6.5 `error` — 2–258 bytes

- first 2 bytes: unsigned error code, big-endian
- remaining 0–256 bytes: optional UTF-8 diagnostic message

Diagnostic text must never be required for machine behavior. When present, the
message must be well-formed UTF-8; malformed text causes the payload to be rejected.

## 7. Audio payload and fragmentation

### 7.1 PCM

`pcm_s16le` payload bytes are signed 16-bit little-endian, interleaved by channel.
For stereo, each sample frame is left sample followed by right sample.

A 20 ms / 48 kHz / stereo PCM frame contains:

- 960 sample frames
- 1,920 individual samples
- 3,840 payload bytes

With the 1,168-byte payload ceiling, this logical frame is sent as four fragments.
The first three may contain 1,168 bytes and the final fragment contains the
remainder. The receiver reconstructs fragments in `fragment_index` order.

### 7.2 Opus

An Opus payload contains one encoded logical audio frame. It normally uses
`fragment_count = 1`; the generic fragmentation fields remain available if an
encoded payload ever exceeds the datagram limit.

## 8. Required rejection rules

A version-1 receiver must reject a datagram when any of these is true:

- datagram is shorter than 32 bytes or longer than 1,200 bytes
- magic is not `WTPK`
- version is not `1`
- packet type is unknown
- unknown flag bits are present
- `stream_id` is zero
- `fragment_count` is zero
- `fragment_index >= fragment_count`
- a control packet is fragmented
- a control packet has non-zero `frame_id` or flags
- declared payload size violates the packet-type schema
- declared payload size does not equal the bytes actually received
- audio payload is empty without the `silence` flag

Malformed packets must be counted for diagnostics and otherwise ignored. They
must not crash the sender or receiver and must not mutate active stream state.

## 9. Versioning and forward compatibility

Protocol version `1` uses strict interpretation:

- incompatible header or semantic changes require a new version number
- version-1 implementations reject other versions
- unknown packet types and unknown flags are rejected
- reserved fields must be transmitted as zero

A future negotiation mechanism may permit multiple versions, but Phase 1 does
not guess compatibility.

## 10. Security boundary

Phase 1 packets are a local development contract, not a release security model.
Discovery, authenticated pairing, replay protection, and encryption are Phase 8
requirements. WireTone V1 must not be publicly released with unauthenticated
plaintext audio transport.
