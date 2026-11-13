param([Parameter(Mandatory)][string]$QtPath, [string]$BuildDirectory = "$PSScriptRoot/../build/windows", [string]$Version = '1.0.0', [string]$CompilerPath = '')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$stage = "$root/artifacts/DroidMediaLink-$Version-windows-x64"
if (Test-Path $stage) {
    $resolvedStage = (Resolve-Path $stage).Path
    $resolvedArtifacts = (Resolve-Path "$root/artifacts").Path
    if (!(Split-Path $resolvedStage -Parent).Equals($resolvedArtifacts, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected staging directory.' }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Force $stage | Out-Null
$binary = Get-ChildItem $BuildDirectory -Recurse -Filter DroidMediaLink.exe | Select-Object -First 1
if (!$binary) { throw 'Build DroidMediaLink.exe first.' }
Copy-Item $binary.FullName "$stage/DroidMediaLink.exe"
Copy-Item "$root/artifacts/native/WindowsNative.dll" $stage
Copy-Item "$root/LICENSE","$root/THIRD_PARTY_NOTICES.md" $stage
Copy-Item "$root/README.md","$root/README.en.md","$root/CHANGELOG.md" $stage
New-Item -ItemType Directory -Force "$stage/docs" | Out-Null
Get-ChildItem "$root/docs" -File -Filter '*.md' | Copy-Item -Destination "$stage/docs"
foreach ($subdirectory in @('screenshots')) {
    if (Test-Path "$root/docs/$subdirectory") { Copy-Item "$root/docs/$subdirectory" "$stage/docs/$subdirectory" -Recurse -Force }
}

$translations = "$stage/translations"
New-Item -ItemType Directory -Force $translations | Out-Null
Get-ChildItem "$root/windows/translations" -Filter '*.ts' | ForEach-Object {
    & "$QtPath/bin/lrelease.exe" $_.FullName -qm "$translations/$($_.BaseName).qm"
    if ($LASTEXITCODE) { throw 'Translation build failed.' }
}
& "$QtPath/bin/windeployqt.exe" --release --skip-plugin-types qmltooling,platforminputcontexts,vectorimageformats --translations en,zh_CN --qmldir "$root/windows/qml" --dir $stage "$stage/DroidMediaLink.exe"
if ($LASTEXITCODE) { throw 'Qt deployment failed.' }
Copy-Item "$QtPath/licenses" "$stage/qt-licenses" -Recurse -ErrorAction SilentlyContinue
$qtLicenseRoot = Join-Path $QtPath '../../Licenses'
if (Test-Path $qtLicenseRoot) { Copy-Item $qtLicenseRoot "$stage/qt-licenses" -Recurse -Force }
Copy-Item "$root/docs/build-and-release.md" $stage
Copy-Item "$root/docs/licenses" "$stage/licenses" -Recurse -Force
New-Item -ItemType Directory -Force "$stage/licenses/liquidglass" | Out-Null
Copy-Item "$root/windows/vendor/liquidglass/LICENSE","$root/windows/vendor/liquidglass/NOTICE","$root/windows/vendor/liquidglass/UPSTREAM.md" "$stage/licenses/liquidglass"

New-Item -ItemType Directory -Force "$stage/licenses/qwindowkit" | Out-Null
Copy-Item "$root/windows/vendor/qwindowkit/LICENSE","$root/windows/vendor/qwindowkit/UPSTREAM.md" "$stage/licenses/qwindowkit"

if (Test-Path "$root/assets/vendor/lucide/LICENSE") { Copy-Item "$root/assets/vendor/lucide/LICENSE" "$stage/licenses/Lucide-LICENSE.txt" }
$sbomDirectory = "$stage/qt-sbom"
New-Item -ItemType Directory -Force $sbomDirectory | Out-Null
foreach ($module in @('qtbase','qtdeclarative','qtsvg','qtwebsockets','qtimageformats','qtshadertools','qttranslations')) {
    Get-ChildItem "$QtPath/sbom/$module-*.spdx.json" -ErrorAction SilentlyContinue | Copy-Item -Destination $sbomDirectory
}
if (!$CompilerPath -and $QtPath -match 'mingw') {
    $gcc = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($gcc) { $CompilerPath = Split-Path (Split-Path $gcc.Source -Parent) -Parent }
}
if ($CompilerPath -and (Test-Path "$CompilerPath/licenses")) {
    foreach ($component in @('gcc','mingw-w64','winpthreads')) {
        if (Test-Path "$CompilerPath/licenses/$component") { Copy-Item "$CompilerPath/licenses/$component" "$stage/licenses/$component" -Recurse -Force }
    }
}
$qtVersion = & "$QtPath/bin/qmake.exe" -query QT_VERSION
$qtDocs = Join-Path $QtPath "../../Docs/Qt-$qtVersion"
if (Test-Path $qtDocs) {
    New-Item -ItemType Directory -Force "$stage/qt-attributions" | Out-Null
    foreach ($module in @('qtcore','qtgui','qtnetwork','qtwidgets','qtqml','qtquick','qtquickcontrols','qtsvg','qtwebsockets','qtimageformats','qtshadertools')) {
        Get-ChildItem "$qtDocs/$module/*attribution*.html" -ErrorAction SilentlyContinue | Copy-Item -Destination "$stage/qt-attributions"
    }
}
Compress-Archive -Path "$stage/*" -DestinationPath "$root/artifacts/DroidMediaLink-$Version-windows-x64.zip" -Force
$compiler = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if (!$compiler) { $compiler = Get-Item "${env:LOCALAPPDATA}/Programs/Inno Setup 6/ISCC.exe" -ErrorAction SilentlyContinue }
if (!$compiler) { $compiler = Get-Item "${env:ProgramFiles(x86)}/Inno Setup 6/ISCC.exe" -ErrorAction SilentlyContinue }
if ($compiler) { $compilerPath = if ($compiler -is [System.IO.FileInfo]) { $compiler.FullName } else { $compiler.Source }; & $compilerPath "/DAppVersion=$Version" "/DStageDirectory=$stage" "$PSScriptRoot/installer.iss"; if ($LASTEXITCODE) { throw 'Installer build failed.' } }
else { Write-Warning 'Inno Setup 6 is required to create the installer. Portable ZIP created.' }
Get-ChildItem "$root/artifacts" -Filter '*.zip' | Get-FileHash -Algorithm SHA256 | Format-Table
