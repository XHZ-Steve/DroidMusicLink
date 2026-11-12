param([string]$OutputDirectory = "$PSScriptRoot/../artifacts/native")
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ tools required.' }
& (Join-Path $installation 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$destination=(Resolve-Path $OutputDirectory).Path
& cl.exe /nologo /std:c++20 /EHsc /MT /O2 /DUNICODE /D_UNICODE "$PSScriptRoot/../windows/native/SessionProbe.cpp" "/Fo:$destination/SessionProbe.obj" "/Fe:$destination/SessionProbe.exe" /link windowsapp.lib runtimeobject.lib
if ($LASTEXITCODE) { throw 'Session probe compile failed.' }
& "$destination/SessionProbe.exe"
exit $LASTEXITCODE
