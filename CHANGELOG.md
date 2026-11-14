# Changelog

## 1.1.0 preview

- Preview4: aligned lyric ink and per-layout title/lyrics/artwork offsets; three alignment presets; timeline-synchronized long-line scrolling, delayed loading feedback and lyric fade-in; more perceptible ambient flow with hidden-window pause.

- Preview3: draggable cover-provider order and independent real-fetch diagnostics; bounded source deadlines, cooldown fallback and transient retries; invalidate incomplete/late-image requests safely.

- Preview2: experimental NetEase and QQ cover sources, configurable priority/enable switches, conservative catalogue matching, source-separated caches and automatic fallback.

- Optional MusicBrainz/CAA high-resolution covers with conservative matching, asynchronous decode, bounded song/album cache, phone-only fallback and quality settings.
- Preserve bounded Android original cover pixels, compare local metadata URIs and deduplicate artwork transmission; no Android artwork network fetching.
- Keep source service limits, rights notices and exact verification limitations in docs/high-resolution-covers.md.


## 1.0.0 — 2026-11-13

- Android-to-Windows authenticated LAN media synchronization, playback control and SMTC artwork.
- Automatic multi-source lyrics, player-published phone lyrics and translation supplementation.
- Ambient artwork colors, optional Liquid Glass, capsule Dock and transparent desktop widgets.
- Persistent settings, current factory preferences and drag-time UI refresh suspension.
- Real-device screenshots and Windows demonstration in bilingual READMEs.


## 0.1.0 — development build

- Android passive TLS/WebSocket service, NSD advertisement and media-session callbacks.
- Explicit phone pairing, certificate pinning, revocable encrypted credentials.
- Windows Qt Quick UI, control bridge, local progress interpolation and native SMTC DLL.
- Local lyrics import, optional LRCLIB queries, settings and runtime translations.
- Protocol schema/vectors, build scripts, portable/installer packaging and CI.

This entry describes implemented code, not blanket completion of the product
acceptance matrix. See `docs/testing.md` and `docs/known-limitations.md` for
executed verification and remaining gaps.

## Release note template

Version / date / exact commit:

- Changes:
- Tested Windows versions / Android devices:
- Known limitations:
- APK signing certificate fingerprint:
- Windows signing publisher (or unsigned):
- Artifact names / SHA-256:
- Dependency/license updates:

## Connection and interface refinement (local test build)

- Device-name discovery and address deduplication; select and compare the same code on both screens, then approve on Android. Removed manual address, fingerprint and code-entry forms.
- Internal SAS commitment/reveal and certificate-bound identities; expired/cancelled attempts do not establish trust, saved credentials stay pinned.
- First-run Android media/battery/startup recommendations; no Android settings search field.
- Optional actual Backdrop AGSL and Qt liquidglass shader materials while retaining native interaction controls.
- Fixed persisted boolean settings and QML value conversion; persistent category navigation and adaptive layouts.
- Independent compact rounded tray menu works while the main window is hidden; left activation only opens the main window.
- Explicit LAN proxy bypass and verified TLS1.2 phone-link compatibility for portable Schannel.
