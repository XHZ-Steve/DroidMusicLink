# Third-party notices

DroidMediaLink source is MIT licensed. This does not change dependency licenses.
No third-party UI framework or GPL application source is copied into this tree.

| Component | Pinned version | License / upstream |
|---|---|---|
| Qt Core, Gui, Widgets, Qml, Quick, Quick Controls, Quick Shapes, Network, WebSockets | 6.11.1 packaging baseline; CMake minimum 6.8 | [LGPL-3.0 / GPL / commercial, module-specific](https://doc.qt.io/qt-6.11/licensing.html) |
| Android Gradle Plugin | 8.13.0 | [Apache-2.0](https://android.googlesource.com/platform/tools/base/) |
| Kotlin / Compose compiler plugin | 2.2.20 | [Apache-2.0](https://github.com/JetBrains/kotlin/blob/master/license/LICENSE.txt) |
| AndroidX Compose BOM | 2025.09.01 | [Apache-2.0](https://android.googlesource.com/platform/frameworks/support/+/androidx-main/LICENSE.txt) |
| Activity Compose | 1.11.0 | Apache-2.0, AndroidX |
| Lifecycle runtime Compose | 2.9.4 | Apache-2.0, AndroidX |
| DataStore Preferences | 1.1.7 | Apache-2.0, AndroidX |
| Java-WebSocket | 1.6.0 | [MIT](https://github.com/TooTallNate/Java-WebSocket/blob/v1.6.0/LICENSE) |
| SLF4J no-operation binding | 2.0.17 | [MIT](https://www.slf4j.org/license.html) |
| JUnit (tests only) | 4.13.2 | [EPL-1.0](https://github.com/junit-team/junit4/blob/main/LICENSE-junit.txt) |
| JSON Schema validator (development only) | 4.25.1 | [MIT](https://github.com/python-jsonschema/jsonschema/blob/main/COPYING) |
| Inno Setup (packaging tool) | installed 6.x | [Inno Setup license](https://jrsoftware.org/files/is/license.txt), installer includes upstream runtime |
| Backdrop (Kyant0/AndroidLiquidGlass) | 1.0.1, tested Compose 1.9 baseline | Apache-2.0; Android APK licenses/Backdrop-NOTICE.txt and Apache-2.0.txt |
| shayann07/liquidglass Qt preview | commit 7f2814a22c2823ab8bc29fd4aa1808d79829b588 | Apache-2.0; windows/vendor/liquidglass/LICENSE, NOTICE, UPSTREAM.md; actual QSB shader, not a full control framework |
| Lucide font and SVG icons | 1.69.0 | [ISC, with Feather-derived icons under MIT](assets/vendor/lucide/LICENSE); original archive and integrity metadata retained |
| QWindowKit Core / Quick | 1.5.1.0, commit `2072958b04820e9e7629f6c40068dee0ce0e9915` | [Apache-2.0](windows/vendor/qwindowkit/LICENSE); native frame only, source attribution retained |
| MinGW libgcc / libstdc++ runtime, local MinGW distribution | 13.1.0 | GPL-3.0 with GCC Runtime Library Exception 3.1; bundled compiler license files retained |
| MinGW-w64 / winpthreads runtime | Qt-supplied MinGW distribution | Component-specific permissive notices, retained in package licenses |

Qt is dynamically linked. Distribution must include the relevant LGPL/GPL
license texts, Qt copyright notices, and an accessible corresponding Qt source
location for the exact shipped version. Users may replace the Qt libraries with
compatible modified builds; no restriction against reverse engineering for
debugging such modifications is imposed. Source:
[Qt 6.11.1 archive](https://download.qt.io/archive/qt/6.11/6.11.1/).
Qt has its own third-party inventory in its installed license files; include
those with the portable folder and installer. Do not distribute a Qt build
without checking its selected module licenses. Do not statically link Qt under
this packaging configuration.

The Android APK is built against Android system APIs. AndroidX transitive
dependencies retain upstream license notices. Both apps use the pinned Lucide icon family. The Android optional optical layer uses the published Backdrop AAR without modification; API 33+ uses its RuntimeShader, older Android retains ordinary Material 3. The desktop optical adapter preserves standard Qt Quick Controls and uses upstream shader sources with the modifications listed in UPSTREAM.md. Qt ShaderTools is a build-time dependency used to bake the GLSL/HLSL/MSL QSB shaders.

Haze (Apache-2.0), FluentUI (MIT original repository),
and braccato (MIT) were evaluated candidates, but are not bundled
dependencies in this revision. Mica For Everyone source is not incorporated.

LRCLIB, NetEase Music, QQ Music and Kugou are optional external lyric services, not bundled software. Their returned
lyrics retain their respective owners' rights. No lyrics are included in this
repository or mirrored by this project.

The native lyric provider adapters, bounded format parsers, lyric ink/fade
shaders and shared layout-transition clock are project implementations. No
braccato, EasyDesktopLyrics, musebase or GPL lyric-project source is copied or
linked. Their architectures/public format descriptions were research references.
KRC decompression uses Qt Core's bundled zlib; its attribution is included in
the deployed Qt third-party notices. Qt Concurrent is part of the dynamically
linked Qt distribution; no browser runtime is introduced.

QWindowKit is statically linked to this executable; Qt remains dynamically linked.
Its Qt private API use is verified against the packaged Qt 6.11.1 build; rebuild
and rerun native frame checks when updating Qt. See `windows/vendor/qwindowkit/UPSTREAM.md`.
