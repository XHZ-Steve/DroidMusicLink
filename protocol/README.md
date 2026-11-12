# Protocol 1

The normative envelope is `v1.schema.json`. UTF-8 JSON is carried as text frames
over WSS on TCP 18123. Android advertises `_droidmedialink._tcp` through NSD;
Windows initiates every connection. Android accepts at most 65,536 characters
per client JSON message and 512 KiB per WebSocket frame; Windows caps incoming
frames/messages at 4 MiB. Android artwork is at most 256 KiB compressed JPEG;
Windows validates compressed length and decoded dimensions before decoding.

Each envelope has integer `protocolVersion: 1`, a unique `messageId`, integer
Unix `timestamp` in milliseconds, `type`, and object `payload`. Unknown optional
payload fields are ignored; unsupported versions are rejected. Milliseconds
are nonnegative integers; volume is a normalized double from 0 to 1.

## Pairing

Windows discovers the phone's advertised name. Selecting it opens WSS and runs SAS pairing version 2; no address, fingerprint or secret-code entry is required. Both screens display their independently computed six-digit comparison value. The phone user must compare and explicitly approve it.

1. PC generates a cryptographic 32-byte client nonce and sends `pair_begin` with `clientId`, `clientName` and `clientCommit = SHA256(clientNonce)` (64 hex characters).
2. Phone generates its own 32-byte nonce, stores a challenge bound to that socket and a 120-second monotonic expiry, and sends `pair_challenge {serverCommit}`.
3. PC sends `pair_reveal {clientNonce}`. Phone verifies the client commitment before revealing `pair_compare {serverNonce}`; PC verifies the server commitment.
4. Each side hashes UTF-8 `DroidMediaLink-SAS-v2` followed by the 32 raw bytes of the actual TLS certificate SHA-256, the raw client nonce, raw server nonce, and UTF-8 clientId. Interpret the first four hash bytes as unsigned big-endian, reduce modulo 1,000,000 and zero-pad to six digits. Neither side trusts a peer-supplied comparison string.
5. Only an explicit phone-side confirmation for the live, unexpired socket produces `pair_approve {clientId,token,deviceId,name}`. PC now saves the certificate pin and DPAPI-protected token and authenticates. Phone-side token storage remains Keystore encrypted. Cancel, denial, timeout and disconnect invalidate the challenge.

Unknown certificates are provisionally accepted only for this comparison exchange. Existing tokens are never sent before the actual peer certificate matches the saved pin. A changed certificate requires a fresh comparison and explicit approval. Credentials are indexed by the certified device identity; discovery can update its network endpoint without changing trust. Discovery names/addresses/TXT values alone never authorize control. Repeated messages, pending requests and command rates are bounded. `pair_cancel` aborts pending comparison; legacy `pair_request` is rejected. Upgrade both apps together for this new handshake, while the envelope remains protocolVersion 1.

No media control is allowed before authorization. Revocation closes active connections and clears rejected stored credentials on PC. Never log tokens or export them with preferences.

## State and controls

`session_snapshot` payload fields: `title`, `artist`, `album`, `appName`,
`packageName`, `sessionId`, `durationMs`, `positionMs`, `speed`, `state`,
`commands`; optional `artwork` is a size-limited base64 JPEG. `state` is
`playing`, `paused`, or `stopped`. Capabilities are exact command names.

`command_request` includes `requestId`, `command`, and, for `seek`,
`positionMs`, or, for `volume`, `volume`. Commands: `play`, `pause`, `toggle`,
`previous`, `next`, `seek`, `volume`. `command_result` carries `requestId`,
`success`, optional `error`. Android serializes execution and rejects repeated
request IDs. Windows drops pending operations on disconnect and times out
unanswered operations. No offline queue is replayed.

Snapshots/position anchors are sent on media events; clients interpolate with a
monotonic clock. They must stop advancing on pause/offline, clamp to duration,
and handle unknown duration. `ping`/`pong` provide liveness, not progress polling.

Reserved event types include `capabilities`, `session_changed`,
`metadata_changed`, `artwork_info`, `playback_state`, `position_anchor`,
`volume_state`, `available_commands`, `lyrics_available`, `lyrics_payload`.
Implementations must not claim these extensions until negotiated.

Errors use stable names: `unauthorized`, `invalid_message`, `unsupported_version`,
`pairing_expired`, `pairing_rejected`, `rate_limited`, `unsupported_command`,
`duplicate_request`, `no_session`, `command_failed`. A malformed or oversized
message may close the connection. Changed certificates require re-pairing.
