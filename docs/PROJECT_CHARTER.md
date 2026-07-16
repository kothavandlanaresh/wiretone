# WireTone Project Charter

## Core purpose

Capture all sound playing on a Windows 11 PC and reproduce it through a Pixel 9a over a
local network with low latency, reliable reconnection, screen-off playback, and no cloud
dependency.

## Primary use case

- YouTube and browser streaming
- films and long-form video
- music and ordinary Windows system audio

## V1 success criteria

1. Reliable Windows WASAPI loopback capture.
2. Local Wi-Fi delivery to one paired Pixel 9a.
3. Screen-off Android playback through a foreground media service.
4. Acceptable lip synchronization for web video.
5. Automatic recovery from short network interruptions.
6. No cloud, account, telemetry, advertising, or analytics.
7. Fully open source and independently buildable.
8. Android distribution through the owner's Google Play Console.
9. Packaged Windows sender requiring no development tools.
10. Measured long-session stability rather than subjective claims alone.

## V1 exclusions

- iOS and iPadOS
- Linux and macOS senders
- internet streaming
- multiple simultaneous receivers
- microphone relay
- screen sharing, file transfer, clipboard, notifications, or remote input
- lossless mode
- monetization work

## Phase rule

A phase advances only with one explicit result: PASS, PASS WITH DOCUMENTED LIMITATION, or
FAIL. “Mostly works” is not a pass.
