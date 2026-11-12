param([string]$NativeDirectory = "$PSScriptRoot/../artifacts/native", [string]$LibraryPath = '')
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ tools required.' }
& (Join-Path $installation 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
New-Item -ItemType Directory -Force $NativeDirectory | Out-Null
$destination = (Resolve-Path $NativeDirectory).Path
if (!$LibraryPath) { $LibraryPath = "$destination/WindowsNative.dll" }
& cl.exe /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /DUNICODE /D_UNICODE "$PSScriptRoot/../windows/native/SmtcArtworkTests.cpp" "/Fo:$destination/SmtcArtworkTests.obj" "/Fe:$destination/SmtcArtworkTests.exe" /link windowsapp.lib runtimeobject.lib shell32.lib user32.lib
if ($LASTEXITCODE) { throw 'SMTC artwork test compile failed.' }
& "$destination/SmtcArtworkTests.exe" $LibraryPath
if ($LASTEXITCODE) { throw "SMTC artwork test failed ($LASTEXITCODE)." }
