# Privacy and permissions

Media metadata and controls travel directly over your authorized LAN link.
Phone audio remains on the phone or its existing Bluetooth output. There are no
accounts, analytics, advertisements, telemetry endpoints, or cloud media storage.

Android notification-listener access is used to obtain active media sessions;
notification message bodies are not collected. A visible connected-device
foreground service provides the passive listener. Network permissions enable
WSS and NSD. Boot receiver restoration remains subject to Android restrictions.
Notification permission lets the system display service/recovery notifications.

On first connection, compare the six-digit numbers on both screens and approve on the phone. Certificate binding remains internal; there are no address, fingerprint or secret-code input fields. Saved credentials are sent only to the pinned identity, and a changed identity requires another explicit comparison. Tokens are encrypted at rest. Remove a paired PC on Android to revoke its authority. Preference exports exclude credentials; Android backups are disabled.

Online lyrics are opt-in and send track title, artist, album and duration to
`lrclib.net`. Release checks contact `api.github.com` and reveal normal network
request metadata such as IP address. No lyric corpus is bundled or mirrored.

Do not forward the phone's listener port to the public internet. The application
does not configure router port forwarding. Diagnostic screenshots can show track
names and short-lived comparison codes; review them before sharing.
