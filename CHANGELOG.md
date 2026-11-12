# Changelog

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
