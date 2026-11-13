# Test record and acceptance matrix

## Executed locally — 2026-11-11

Environment: Windows x64 host, Qt 6.11.1 MinGW, GCC 13.1 toolchain, MSVC
19.44 / Windows SDK 10.0.26100.0 for the native bridge, JDK 17, Gradle 8.14.3,
Android SDK 36, Android emulator 36.2.12 running Pixel 2 XL API 30.

| Check | Evidence | Result |
|---|---|---|
| JSON Schema protocol vectors | `python scripts/check-protocol.py` | 8 valid + 13 rejection cases pass |
| Native SMTC DLL | `scripts/build-native.ps1` | MSVC compilation/link successful |
| Localization resources | `python scripts/check-resources.py` | Android bilingual keys match; 109 finished Qt translations |
| Windows application / parser tests | `artifacts/windows-build`, CTest | Built; core_tests 1/1 pass (Windows agent evidence) |
| Android debug build / unit tests | Gradle assembleDebug/testDebugUnitTest | Latest APK built; 5 unit tests pass; lint 0 errors / 19 warnings (android/testing/redesign-verification.txt) |
| Android installation/start | API 30 emulator | Installed and launched; screenshots from runtime |
| Windows screenshots | `artifacts/screenshots/` | Actual QQuickWindow captures at 4 sizes plus tray/floating/desktop surfaces |
| Android screenshots | `android/screenshots/` | Actual emulator screens, home/device views |
| Independent security review | Local security review notes | Defects reported and iteratively fixed |
| Native TLS handshake | Emulator WLAN listener through local test proxy | Initial Keystore RSA padding and WebSocket subprotocol failures reproduced and fixed; pinned TLS and unauthorized-control rejection then passed |
| Windows package dependency smoke | PATH restricted to Windows system directories | Portable startup, silent per-user setup, installed startup and silent uninstall passed |
| Icon provenance | Lucide upstream archive / integrity | Root agent verified npm SHA-512 integrity and exact upstream TTF, 2,317 SVGs / 2,337 font codepoints |

No physical-device music playback or system-flyout validation is represented
by the protocol fixtures. Fixtures contain clearly synthetic metadata and are
displayed only during explicitly labelled TEST SESSION integration runs, never as real music.

## Reproduce basic checks

```powershell
python -m pip install -r scripts/requirements.txt
python scripts/check-protocol.py
ctest --test-dir artifacts/windows-build --output-on-failure
./scripts/build-native.ps1
gradle -p android testDebugUnitTest lintDebug assembleDebug
```

The optional Android WSS smoke test uses `websockets==15.0.1` and `scripts/test-android-link.py --url wss://127.0.0.1:18123`. It implements the current SAS commitment/reveal exchange, asks the tester to compare the displayed code with the phone and approve there, and never saves the token. An emulator listener bound to WLAN may need a test-only local relay for ADB forwarding; this is test infrastructure, not a product networking requirement.

## Required manual acceptance

1. Pair/deny/revoke/re-pair two real PCs, wrong code, changed certificate,
   oversized/malformed/replayed traffic, and command floods.
2. Grant/deny/revoke notification-listener permission; play two actual Android
   players, switch sources, seek/pause/next, stop the player and disconnect Wi-Fi.
3. Verify phone audio/Bluetooth route remains unchanged. Lock phone, reboot,
   change networks, kill the service, and verify the legal recovery path.
4. Verify Windows system media flyout metadata/artwork/buttons/seek and actual
   media keys with no Windows audio playback.
5. Test tray/mini-player/desktop lyrics, keyboard focus, screen readers, 100–200%
   DPI, multiple monitors, fullscreen and Windows 10 material fallback.
6. Check missing/extreme artwork, long metadata, light/dark/high contrast,
   enlarged Android fonts, disconnected and unknown-duration streams.
7. Search/import/bind lyrics, handle offline/rate limits, seek through timed
   lines/words and compare both lyric surfaces and offsets.
8. Install/uninstall portable and setup distributions on a clean Windows user,
   install the test APK, migrate settings, export without credentials, and test
   unavailable/released GitHub update responses.
9. Measure idle/visible/hidden CPU/GPU/memory/network plus Android screen-off
   battery behavior. Record device/OS, duration, workload and actual numbers.

Release acceptance remains incomplete until these conditions have evidence.

## SMTC system-session integration — 2026-11-11 16:01 CST

Windows 11 Home build 26200 host run, not mocked setters: Android API 30 instrumentation created the
explicit `TEST SESSION` MediaSession without audio. The Windows Qt
`link_probe --native --fixture` used the real WSS service, pinned the phone
certificate, and received phone-side pairing approval. Its visible native
window used the process identity `DroidMediaLink.Desktop`.

- `GetForWindow` initialization and metadata/timeline setters succeeded.
- `SessionProbe.exe` queried the actual
  `GlobalSystemMediaTransportControlsSessionManager`: total sessions 2, target
  sessions 1, source identity `DroidMediaLink.Desktop`, title `TEST SESSION`,
  Playing status 4 followed by Paused status 5.
- `SessionProbe.exe --play` and `--pause` each returned system control accepted
  1. Android `dumpsys media_session` independently showed the fixture change to
  state 3 (playing) and state 2 (paused).
- `SessionProbe.exe --seek-fixture` requested **57000 ms**, distinct from the
  direct Qt fixture's 42000 ms. Android reported paused position 57000 and the
  subsequent Windows system-session query also reported `PositionMs=57000`.
- The Qt probe passed its authenticated 62-second heartbeat survival check,
  then disconnected and exited. At 16:02:11 the system session count returned
  to 1 with target count 0, as expected after the native owner exited.
- No Windows audio playback, silent stream, MediaPlayer, or hidden player was
  created. Evidence: `artifacts/smtc-live.stderr.log` and
  `artifacts/smtc-system-controls.log` (local artifacts, no pairing credentials).
- Android instrumentation completed naturally after its inspection hold:
  `OK (1 test)`, 233.542 seconds; result recorded in
  `artifacts/smtc-android-instrumentation.log`. The temporary relay and ADB
  forwarding were then removed.

The earlier target-count-0 observation does **not** establish an audio-session
requirement: it did not retain a confirmed live fixture/native owner. This
run reproduced target count 0 before the connection and after owner shutdown,
but target count 1 while connected. Microsoft's
[UWP SMTC sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-universal-samples/systemmediatransportcontrols/)
notes an initial audio-playback requirement for that sample; the desktop
Win32 interop run above succeeded without adding audio and should not be
conflated with that sample's behavior.

Reproduction infrastructure: explicitly target `adb -s emulator-5554`; start
instrumentation with `-e holdSeconds 180`; forward host TCP 18123 to emulator
TCP 18124. The Android test-only relay was
`toybox nc -s 127.0.0.1 -p 18124 -L toybox nc 10.0.2.16 18123` because the real
service binds its WLAN address. For the current SAS-v2 build, start the Qt probe with the URL, compare its generated code with the phone and approve on Android. While the Qt
probe remains connected, query `SessionProbe.exe`, then run its `--play`,
`--pause`, and `--seek-fixture` options. These options refuse to control any
target whose title is not the explicit `TEST SESSION` fixture.

This validates system-session publication and system API control forwarding.
The actual Windows flyout appearance/artwork, physical media keys, and a real
third-party Android music player remain manual acceptance items.

## Final build-tree and idle checks

A fresh CTest run initially could not locate Qt DLLs in the build tree when invoked without a developer PATH. The Windows CMake test environment now prepends the linked Qt Core directory and compiler-runtime directory and limits the test to 30 seconds. After reconfiguration, both the developer and independent reviewer reran the ordinary CTest command: **1/1 passed, 0.25 seconds**. End-user packages already bundle these runtime DLLs.

Android final APK: `artifacts/DroidMediaLink-0.1.0-android-test.apk`, test signed. Final build, four unit tests and lint completed; lint has zero errors and 14 warnings. API 30 force-stop followed by Activity launch restored `PassiveLinkService` with `isForeground=true` / `startRequested=true`. See `android/testing/final-verification.txt` and `reopen-service.txt`.

A short Windows hidden/disconnected idle sample measured 0 ms additional process CPU over 8.0047 seconds, working set 72.89 MiB and private memory 41.83 MiB on this 32-logical-processor host. This is a short idle sample, **not** a connected, GPU, screen-off or battery benchmark. Raw result: `artifacts/windows-hidden-performance.json`.

Final desktop interaction check: the packaged app launched with its real native window, exposed labelled controls via Windows accessibility, opened Settings and switched the light theme live. The light-theme placeholder contrast was corrected and checked in `artifacts/screenshots/windows-settings-light.png`; the native titlebar now follows the appearance setting. ComboBox indicators use the same full Lucide font. The temporary light-theme preference was restored afterward.

Final package smoke was repeated after the final light-theme and titlebar fixes: portable startup, per-user setup into artifacts/install-final, installed startup and uninstall all passed with only Windows system directories on PATH. Captures: artifacts/screenshots/final-portable-smoke.png and final-installed-smoke.png. Final binaries and SHA-256 values are listed in artifacts/DELIVERY.md and artifacts/SHA256SUMS.txt.

## Connection and material redesign verification

- Android: complete Gradle build, 5 JVM tests pass, lint 0 errors / 18 warnings. API30 native Qt SAS match/phone rejection/trusted reconnect/62-second heartbeat passed. API33 emulator separately installed and ran the real Backdrop 1.0.1 AGSL lens/blur navbar; the scrollable form is the actual sampled backdrop. First-run recommended media/battery/startup dialog, device page without addresses/fingerprints/codes, and settings without search were captured. See `android/testing/redesign-verification.txt` and `android/screenshots/*api33.png`.
- Windows settings: repaired both QJSValue input handling and persisted Windows string booleans (`"false"` must return a QVariant Bool, not a nonempty QML string). Core tests include JavaScript calls and reopening settings with raw string booleans; actual live theme switching and persistent reload were checked. Settings categories remain below the search box while the options scroll; the playback dock only occupies the music page.
- Windows optics: pinned liquidglass Qt preview commit, qsb-baked HLSL/GLSL/MSL, and actual D3D11 / RTX5070Ti on/off render comparison. The upstream test scene changed 452333 pixels; the app material uses the same real refraction/dispersion shader over supplied QML background content. No desktop screen capture is performed. Evidence: `artifacts/liquidglass-hardware-smoke.log`, hardware on/off PNGs, and redesigned app/window PNGs. ShaderEffect status alone is not used to claim success.
- Tray regression: `DroidMediaLink.exe --tray-smoke` emits the actual Qt Context/Trigger signal routes. With main window hidden, Context displays the independent rounded 320x326 window; left Trigger hides that menu and displays the main window only. Both assertions passed and a real window image was saved. This is a signal-route/runtime-window test, not a claim of physically clicking every taskbar orientation.
- Discovery: a test-only DNS-SD advertisement named `Android emulator TEST` pointed at the real Android emulator WSS relay. Twelve identical announcements produced one entry for that address. The genuine emulator NSD name was also visible at its different network address. No test devices are built into the product. `scripts/probe-discovery.py` documents this fixture.
- Portable TLS: actual Schannel runtime failed with automatic protocol negotiation against API30 while the development environment's TLS backend differed. Explicit TLS1.2 was verified with the portable runtime; peer/SAS verification and native cipher policy remain enabled. Phone WSS and mDNS bypass HTTP/SOCKS proxies; internet lyric/update requests keep their existing network behavior. Desktop and phone both visibly showed code 253296, then phone approval led the desktop UI to Connected. Screenshots: `pairing-desktop-matched.jpg`, `pairing-android-matched.png`. The code is expired, not an authentication token.
- Pairing security was independently reviewed read-only in local pairing review notes. The envelope schema now rejects legacy pair_request and validates all new commitments/nonces.

Physical third-party-player/Bluetooth checks and broad router/DPI/GPU matrices remain separate from these emulator and local runtime results.

Final redesign regression: CTest 1/1 passed in 0.28 seconds after the transport and material changes. Tray background Context and left Trigger assertions both passed again (`artifacts/tray-background-smoke-final.log`). The smallest 560x520 player and 800x700 settings page were rendered and inspected without overlap; categories stay outside the scrolling options area. Live controls were exercised through the Windows accessibility/UI path; the tests use separate INI profiles rather than modifying normal paired identities. The portable Schannel handshake and phone approval were subsequently checked in the actual app, not only the developer CLI probe. Initial relay failures were test infrastructure failures, not evidence of a phone-service crash; the relay was restored before the successful runs.

Delivery Android rebuild after the API-availability guard: build/unit tests/lint succeeded, 5 tests, 0 lint errors and 19 warnings. Both API30 and API33 final APK installations and Activity launches succeeded. API30 UI hierarchy confirms that the unsupported optical switch is disabled and unchecked; its help remains available. API33 retains the enabled optical style. Evidence: `artifacts/android-redesign-delivery-build.log` and `artifacts/dml-low-api.xml`.

The final redesigned Windows package also passed system-only-PATH portable launch (Liquid Glass selected), installation, installed launch at 560x520 and uninstall. No extra Qt/OpenSSL developer directories were needed. Shader/load logs were empty; actual captures are `redesign-portable-final.png` and `redesign-installed-final.png`.
# Physical phone connection debugging — 2026-11-11

- Device: user's PLU110, accessed only by its explicitly supplied ADB serial. The two test emulators (`emulator-5554`, `emulator-5556`) were stopped; ADB then listed only the physical phone.
- The phone's foreground service was running and its Wi-Fi TCP 18123 was reachable directly from Windows. No ADB forwarding or simulated DNS-SD records were used for this test.
- Before the fix, `link_probe --discover` returned no device with WLAN and a virtual adapter active. After joining IPv4 mDNS and sending the query on every eligible up/running multicast interface, it found the real `PLU110` endpoint. The same URL remains one device entry.
- The real Bridge client completed SAS comparison against the phone dialog, explicit approval, authentication, 62-second heartbeat survival, and saved-certificate/token reconnect without a second pairing prompt. No third-party playback commands were sent.
- The normal desktop profile also completed a separate matching-code approval with this phone, saving its device identity for the actual app. `--diagnose-link` logs connection state and the temporary comparison code only, never credentials or media contents.
- Windows rebuild and `core_tests`: 1/1 passed. Independent read-only review found no blocking issue; repetitive discovery logging was reduced to one line per URL.
- Windows automation helpers were unavailable during this session; the app's own Qt capture path is used for the connected-window screenshot. This is not a claim of physical mouse testing.

# Physical media synchronization and paused controls — 2026-11-11

- Reproduced on the user's API36 PLU110: Salt Player had a PLAYING session and notification-listener authorization was granted, but the listener service had `app=null`. Because tracking previously started only in `onListenerConnected`, the desktop received no media.
- Moved session observation/control to the foreground link service, using the explicitly authorized listener component with MediaSessionManager. Listener connection/disconnection and activity resume now refresh tracking and request rebind without polling. The grant button observes actual permission state.
- After an in-place APK update preserving pairing/data, the real Windows application displayed live third-party title, artist, cover, progress and enabled controls while the listener remained unbound. Source labels now have scoped launcher-package visibility.
- `scripts/test-live-media.py` used actual authenticated WLAN WSS, compared its SAS with the phone, and verified actual Salt Player pause, next while paused, previous while paused, play from paused, and restoration of the original track. Results: `artifacts/physical-live-media.json`; titles and credentials are not written to the report.
- Real-device instrumentation `PausedMediaControlTest`: 1/1 passed. A paused, audio-free TEST MediaSession advertising zero action bits still received all four transport callbacks (play, pause, next, previous). This verifies controls do not disappear just because a player omits state-dependent capability flags.
- No-current-session snapshots explicitly clear old metadata/artwork and expose only the basic system-key fallback when source restrictions permit. Actual resumption with every player having no active session was not claimed; Android may have no resumable target.
- Android `testDebugUnitTest lintDebug assembleDebug`: successful; 5 unit tests; lint 0 errors/19 warnings. The temporary instrumentation APK and temporary test peer authorizations were removed after testing; the real desktop pairing remains.

# Windows visual system and rendering — 2026-11-12

See `windows-ui-design.md` for the final design, component integration, screenshots, measured CPU/memory/frame-cadence results and remaining platform coverage. The implementation was built and run, including native mouse edge resizing, maximize/restore, actual theme clicks, isolated real-album color tests, settings end-scroll and authenticated phone playback-control state verification. Raw artifacts remain in `artifacts/ui-review`, `artifacts/ui-live-transport`, `artifacts/ui-profiles/bindings`, and `artifacts/ui-audit-{100,150,200}`. Test artwork/lyrics previews are explicitly isolated and do not synthesize production media state.


## Windows lyrics, navigation and native-frame refinement — 2026-11-12

See `windows-refinement-2026-11-12.md` for current integration tests, measured tray latency and resize stress results. Earlier UI reports describe their original build, not this revision.

## Phone lyric source and ui9 interaction regression — 2026-11-13

See [ui9 verification](ui9-phone-lyrics-and-interactions.md) for provider-chain tests, repaired CI assertions, actual packaged runtime evidence and remaining physical-device coverage.
