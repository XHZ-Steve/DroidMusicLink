# Dependency decisions — 2026-11-11

Versions are intentionally pinned for reproducibility, not claimed to be the
newest versions available. Local compiler/build output is the compatibility
evidence; upstream documentation supplies API and licensing evidence.

- [AGP 8.13 official compatibility](https://developer.android.com/build/releases/agp-8-13-0-release-notes):
  maximum API 36.1, Gradle minimum 8.13, JDK 17, default Build Tools 35.0.0.
  The project uses AGP 8.13.0 / Gradle 8.14.3 / JDK 17 / API 36.
- [Compose BOM](https://developer.android.com/develop/ui/compose/bom): aligns
  Compose modules. Compiler plugin is Kotlin's matching 2.2.20 plugin; the BOM
  does not manage the compiler. BOM is pinned to 2025.09.01.
- [Java-WebSocket upstream](https://github.com/TooTallNate/Java-WebSocket):
  pure Java WebSocket server, MIT, pinned 1.6.0. Standard JSSE TLS backed by
  Android Keystore is used rather than a custom cryptographic transport.
- [Qt licensing](https://doc.qt.io/qt-6.11/licensing.html) and
  [LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations):
  dynamically linked libraries, preserved license files, replaceable DLLs,
  exact source archive link. Actual installed baseline is 6.11.1.
- [Win32 SMTC interop](https://learn.microsoft.com/en-us/windows/win32/api/systemmediatransportcontrolsinterop/nf-systemmediatransportcontrolsinterop-isystemmediatransportcontrolsinterop-getforwindow):
  requires a top-level HWND in the calling process. The native bridge follows
  this model and creates no audio stream.
- [QWindowKit](https://github.com/stdware/qwindowkit): Apache-2.0, maintained
  project evaluated. Not imported; standard Qt window interaction is retained.
- [Haze](https://github.com/chrisbanes/haze): Apache-2.0, evaluated but not
  integrated because this revision has not verified a compatible Haze/Compose
  build. The Android UI uses one native Material 3 stack.
- [Inno Setup](https://jrsoftware.org/isinfo.php): upstream installer compiler
  6.7.3 installed through winget with package hash verification. Check upstream
  license terms when using the compiler commercially.

No source from Mica For Everyone or other GPL applications has been copied.
The deployment script explicitly excludes virtual-keyboard, QML debugging, and
vector-image-format plugin groups to avoid unnecessary modules. Qt SBOM files
for the bundled module families are copied to the package for inventory review.

Optional optical layers added in the connection redesign: Backdrop 1.0.1 (Kyant0/AndroidLiquidGlass, Apache-2.0, tested Compose1.9 baseline / API33 shader, older Android standard fallback), and shayann07/liquidglass Qt preview commit7f2814a22c2823ab8bc29fd4aa1808d79829b588 (Apache-2.0, actual D3D11/qsb validation). Standard interaction controls remain Material3/Qt Quick Controls; no second complete control framework was introduced. See the retained NOTICE/LICENSE and Qt vendor UPSTREAM.md for adapter changes and preview status.
