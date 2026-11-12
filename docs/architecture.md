# Architecture

Android is a passive WSS server. `MediaMonitor`, authorized as a notification
listener, subscribes to standard `MediaSession` callbacks and selects an active
controller. `PassiveLinkService` owns the TLS listener and NSD advertisement;
network callbacks rebuild the listener when the Wi-Fi address changes. Commands
are marshaled to Android's main handler so transport actions are serialized.
No audio data leaves the phone.

Windows `Bridge` owns a single media state shared with QML, tray and native
SMTC. Qt Network/WebSockets handle transport. Progress uses an elapsed monotonic
timer from the latest event anchor; a UI timer changes presentation only.
Backoff reconnect has jitter. Credentials are encrypted with Windows DPAPI;
Android pairing records use AES-GCM with an Android Keystore key.

`WindowsNative.dll` is a small C ABI boundary, compiled by MSVC against the
Windows SDK. It uses `ISystemMediaTransportControlsInterop::GetForWindow` and
C++/WinRT without creating an audio stream. Qt may be built with MinGW or MSVC;
the native DLL uses no C++ objects across the compiler boundary. Event callbacks
are delivered back to Qt through its queued invocation mechanism.

`LyricsEngine` currently lives in Bridge: local LRC/TTML import, optional LRCLIB,
and a per-track disk cache. Preferences and credentials are separate namespaces;
only preferences are exported. UI rendering is suspended when windows hide.

This repository does not use cloud accounts, audio redirection, Root, ADB at
runtime, or a browser rendering engine. ADB is only a development/test tool.
