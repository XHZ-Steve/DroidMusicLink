# Windows refinement — 2026-11-12

## Delivered implementation

- Native Qt Quick views and Controls remain the interaction layer. QWindowKit 1.5.1.0 (pinned Apache-2.0 source) provides native frame hit-testing, resize, caption drag and Snap Layout. The main canvas has no permanent title/navigation/status. Caption buttons and the settings gear reveal near the top edge. Device management lives in settings.
- Six settings groups and 25 subsections, all-setting search with target navigation, retained section scrolling, Alt+Left navigation history, section/all reset, validated live settings and JSON v2 export/import (v1 import supported). Device credentials are excluded from export and survive resets.
- Full-resolution analytic album atmosphere with distinct color populations, bounded saturation/vibrance, stable spatial dither and no full-window grey overlay. Scoped inherited/preset material configuration drives real shader uniforms. The pinned liquidglass port supplies optical refraction, distortion, dispersion and edge highlights; standard style remains available.
- Volume is an anchored popup using the actual reported phone volume. Slider/mute commands use authenticated transport and rebind to reported state; they do not synthesize successful device updates. Equal expanding Dock side regions keep transport centered.
- Automatic lyrics use durable manual mapping, cache and embedded phone lyrics before optional LRCLIB. Requests are aborted/generation guarded on song changes. Native shared parsing/timeline/media clock drives independent main/desktop views; stable line delegates and clipped word fills avoid rebuilding rich text every tick.
- Desktop lyrics use an explicit requested state, pause/empty/fullscreen policies, hover editing, remembered drag position, click-through, native text outline/shadow and optional scoped glass background. Hidden views unregister their lyric clock and release delegates; desktop scene-graph persistence is disabled. Foreground fullscreen detection uses WinEvent hooks, not a polling loop.

## Actual verification

- Qt 6.11.1 / MinGW / Windows 11 25H2 / D3D11 build; CTest: core, parser and asynchronous engine tests passed. Engine tests include late duration metadata, backward seek, stale HTTP callbacks, durable manual mappings and cache priority. Live LRCLIB matching and cached replay loaded 28 real synchronized lines without manual selection.
- QML settings editor tests: 9 passed, covering real switch, numeric, enum and shortcut input; scoped inheritance; search/advanced reveal and action routing. Resource parity and protocol acceptance/rejection vectors passed.
- Real authenticated phone: two actual Qt Dock clicks changed paused → playing → paused, received successful command acknowledgments and restored the original play state. The phone was reachable on LAN while USB ADB was unavailable during this final run. Actual phone lyrics loaded automatically in the running UI.
- Native frame smoke: HTCAPTION, HTLEFT, HTMAXBUTTON, minimum tracking size, maximize, restore, minimize and close-to-tray all passed. Actual GUI checks covered settings shortcut/two-level navigation and anchored glass volume popup.
- Repeated real-lyric lifecycle tests: 300 show/hide/edit transitions and 30 model resets per 36-second run. A reopen-position defect found in screenshot inspection was corrected and the run repeated successfully. Transparent window, main view and edit-mode PNGs were captured.
- Tray Context timing on the integrated build: first requested frame 28.72ms; reopen 11.16ms; same native HWND reused, no prewarm focus activation. Startup prewarm was 370.26ms and total startup readiness 784.80ms; these startup costs are not hidden in the right-click latency figure. DXBC is embedded at build time using Windows SDK fxc.

## Stability evidence and limits

The old implementation rebuilt/replaced lyric rich text on coarse progress ticks and did not isolate a shared visible-surface lyric clock. Those hot paths were replaced. No native crash dump reproducing the originally reported desktop-lyrics crash was available: an exact original native crash root cause is **not established**. Lifecycle guards and repeated real-lyric tests passed; this is not a guarantee across every driver or long-duration workload.

braccato (MIT), EasyDesktopLyrics (MIT) and musebase (MPL-2.0) were reviewed as architecture references. No code was copied from them, and no browser/Avalonia/WPF runtime was added. Liquidglass, QWindowKit, complete Lucide assets and their pinned attribution/license files remain in the source tree and package.

Additional independent review could not run after the existing agents exhausted their account quota. This final integration is backed by executed tests and direct GUI inspection, not a claimed new independent review. Mixed-DPI monitor migration, HDR, all drivers and a prolonged overnight soak remain unverified. Only LRCLIB is integrated as an online provider; translation/romanization/word timing depend on the lyric data supplied. Installers remain unsigned.

Raw reports and screenshots are under `artifacts/`: `native-frame-integrated.json`, `tray-latency-integrated.json`, `live-integrated/transport-report.json`, `lyrics-soak-integrated-v2/`, `audit-current-{100,150,200}/` and `ui-refinement-review/`.

## Resize/performance measurements on the final integration

Each run changed the real window size 240 times with synchronous screenshot readback. CPU is percent of **one core**, not whole-machine CPU. Presented-frame intervals are not GPU render duration. These are stress diagnostics, not steady playback benchmarks.

| DPR | Texture/geometry mismatches | Dock center error | Background max adjacent-channel delta | Frame interval P95 | CPU / one core | Working set | Hidden CPU / one core |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1.0 | 0 | 0.5px | 1 | 12.94ms | 25.51% | 176.90MiB | 0.78% |
| 1.5 | 0 | 0.5px | 1 | 13.05ms | 34.56% | 188.05MiB | 0.78% |
| 2.0 | 0 | 0.5px | 1 | 13.49ms | 39.18% | 183.84MiB | 0.00% |

Thirteen final isolated Qt captures cover warm/cool/saturated/monochrome/pale artwork, light/dark, narrow/minimum, settings, discovery popup, floating player and tray. Previews do not fabricate production song metadata. The final real-phone playing capture is in `docs/screenshots/refinement/`.
