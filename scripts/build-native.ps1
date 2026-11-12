param([string]$OutputDirectory = "$PSScriptRoot/../artifacts/native")
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ x64 tools are required.' }
$developerShell = Join-Path $installation 'Common7/Tools/Launch-VsDevShell.ps1'
& $developerShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$destination = (Resolve-Path $OutputDirectory).Path
$source = Join-Path $PSScriptRoot '../windows/native/WindowsNative.cpp'
& cl.exe /nologo /std:c++20 /EHsc /MT /O2 /LD /DUNICODE /D_UNICODE $source "/Fo:$destination/WindowsNative.obj" /link windowsapp.lib runtimeobject.lib "/OUT:$destination/WindowsNative.dll" "/IMPLIB:$destination/WindowsNative.lib"
if ($LASTEXITCODE) { throw "Native bridge build failed ($LASTEXITCODE)." }
