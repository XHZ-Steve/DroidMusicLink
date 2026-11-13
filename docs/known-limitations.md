# Known limitations

This is an implemented development build, not a claim that every requirement
in the complete product acceptance matrix has passed acceptance.

The Windows UI redesign passed the 2026-11-12 runtime matrix described in
`windows-ui-design.md`, including native edge resizing and 100/150/200% optical
geometry stress. Mixed-DPI multi-monitor dragging, all GPUs/HDR and a complete
screen-reader matrix remain outside the verified coverage.

- A physical PLU110 phone passed LAN discovery, SAS pairing, authentication,
  heartbeat and saved-identity reconnection. Salt Player metadata/artwork and
  real pause/play plus next/previous while paused also passed on this phone.
  Other players, Bluetooth audio, OEM battery management and reboot/locked-phone
  matrices remain unverified.
- Windows SMTC publication and system API play/pause/seek forwarding passed
  against the real Android instrumentation MediaSession over authenticated WSS,
  without Windows audio playback. System-flyout appearance/artwork, physical
  media keys, and broader third-party Android player coverage still need testing.
- Android API 30 emulator coverage does not establish Android 15/16/17 foreground
  service behavior across devices. The user's API36 PLU110 passed foreground
  media-session tracking even with its approved notification listener unbound.
  This build targets 36 and does not claim target 37's local
  network runtime-permission path.
- With no current media session, basic controls submit Android system media key
  events. Android/the player must retain a resumable media target for these to
  have an effect; a successful submission is not proof of resumed playback.
  Manual source/exclusion restrictions disable global-key fallback rather than
  accidentally controlling a different or excluded app.
- DNS-SD is IPv4-focused. IPv6-only networks, cache TTL/expiry, multi-interface
  discovery and sleep/wake require broader validation. WLAN discovery with a concurrent virtual adapter passed on the physical phone after explicitly joining/sending on each eligible interface. Credentials use certified device identities; discovered endpoint changes update saved addresses. This has not yet passed every router, IPv6 and DHCP-transition matrix.
- Full production update downloading, cryptographic artifact verification,
  rollback and APK installation UX are not delivered. Version checks link to
  existing GitHub release metadata and handle no release/network errors.
- Settings now use a two-level descriptor registry, scoped material overrides, presets and section reset. Search is literal localized text/key matching; comprehensive pinyin/fuzzy search and every taskbook option are not implemented.
- TTML support handles basic paragraph timing; enhanced LRC word timing is
  data-dependent. Rich TTML ruby/translation roles and every timing syntax are
  not claimed. Only the optional LRCLIB provider is integrated.
- Native background materials and custom floating windows have not passed the
  complete Windows 10/11, DPI, full-screen, click-through and multi-monitor matrix.
- Haze is not bundled. QWindowKit 1.5.1.0 is now statically integrated for the Windows frame. Optional Backdrop 1.0.1 optics were actually run on API33; Android below API33 uses standard Material 3. The optional liquidglass Qt port is an upstream preview pinned to a commit and tested here with D3D11, not a claim of broad GPU compatibility.
- No production signing identity was provided. Android APK is test-signed;
  Windows setup and executables are unsigned. Signing and publication remain
  distributor responsibilities; no remote Release was created.
- CPU/GPU/memory/network/battery baselines across visible/hidden and phone
  screen states have not all been measured. Do not infer efficiency numbers
  from event-driven architecture alone.

Use `docs/testing.md` for executed evidence and local review notes for incremental
findings, including fixed defects and remaining verification.
