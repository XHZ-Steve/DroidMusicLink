# Build and packaging

## Windows x64

Install CMake 3.24+, Ninja, Qt 6.11.1 (Core/Gui/Widgets/QML/Quick/QuickControls2/
QuickDialogs2/Network/WebSockets/ShaderTools and matching Qt private headers), and Visual Studio 2022 C++ x64 build tools
with a Windows 10/11 SDK including x64 `fxc.exe`. Qt and C++ compiler ABI must match. The native SMTC
DLL always uses MSVC; Qt's C ABI loader also works with MinGW.

QWindowKit Core/Quick is pinned in `windows/vendor/qwindowkit` and built from
source against the exact Qt installation. The build script locates Windows SDK
`fxc.exe` and compiles the optical/ambient HLSL to DXBC at build time. A manual
CMake build must also put the SDK x64 bin directory on PATH; the SDK compiler is
a build dependency and is not redistributed.

From PowerShell with the matching Qt compiler on PATH:

```powershell
./scripts/build-windows.ps1 -QtPath D:/programs/Qt/6.11.1/mingw_64
./scripts/package-windows.ps1 -QtPath D:/programs/Qt/6.11.1/mingw_64
```

Local MinGW tools are `D:/programs/Qt/Tools/mingw1310_64/bin` and Ninja is in
`D:/programs/Qt/Tools/Ninja`. Qt officially determines compiler compatibility;
do not substitute a different major ABI. MSVC builds need a developer shell.
Install Inno Setup 6 to produce the per-user installer. Portable ZIP and setup
EXE are written into `artifacts`. Portable deployment includes Qt DLLs and QML
plugins via `windeployqt`. Distributions are unsigned unless the distributor
adds Authenticode signing with their own certificate. Never check a PFX into Git.

## Android

JDK 17, Gradle 8.14.3, Android SDK platform 36 and Build Tools 35.0.0 are needed.
Set `ANDROID_HOME` or untracked `android/local.properties` (`sdk.dir=...`).

```powershell
gradle -p android testDebugUnitTest lintDebug assembleDebug
```

The debug APK at `android/app/build/outputs/apk/debug/app-debug.apk` uses the
local development debug key. It is installable but is **not production signed**.
Do not distribute the debug key as a production identity. Production release
requires a private upload/signing keystore supplied outside the repository,
`assembleRelease`, zipalign/apksigner, and independent installation validation.
Keep the same production identity for future updates. Never commit credentials.

Minimum Android API is 26; target/compile API is 36. Target 37 local network
permission behavior is not claimed by this build. Test it before raising target.

## Checks and publication

```powershell
python -m pip install -r scripts/requirements.txt
python scripts/check-protocol.py
```

CI builds both platforms and uploads artifacts. No workflow pushes a Git tag,
creates a Release, or publishes automatically. Before manual publication, run
the real-device matrix in `testing.md`, audit license files, sign the packages,
compute SHA-256, and attach release notes. Client update checks point to
`XHZ-Steve/DroidMusicLink` GitHub Releases. A missing release is a normal error.
