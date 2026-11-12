# Design system

Both apps use a muted cyan/neutral palette, large primary media hierarchy,
rounded controls, and generous spacing. Album colors may tint the background
without affecting text contrast. Windows uses Qt Quick; Android uses Compose
Material 3. No additional full UI framework is embedded.

Main actions remain visible; help belongs in a tooltip/dialog. Icon actions
need accessible names. Empty states offer the next functional action. Connection
and authorization status are explicit; cached metadata must not imply a live
connection. Destructive credential revocation requires a consequence prompt.

Animation stops with hidden windows/background activities. Large blur effects
must be optional and respect reduced motion/transparency preferences. Each
theme must be reviewed from actual runtime screenshots before acceptance.
