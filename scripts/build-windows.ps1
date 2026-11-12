param([Parameter(Mandatory)][string]$QtPath, [string]$BuildDirectory = "$PSScriptRoot/../build/windows", [string]$Generator = 'Ninja')
$ErrorActionPreference = 'Stop'
# Qt ShaderTools PRECOMPILE embeds DXBC, avoiding D3DCompile on first tray open.
if (!(Get-Command fxc.exe -ErrorAction SilentlyContinue)) {
    $sdkShaderCompiler = Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/fxc.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (!$sdkShaderCompiler) { throw 'Windows SDK x64 fxc.exe is required to precompile shaders.' }
    $env:PATH = "$($sdkShaderCompiler.DirectoryName);$env:PATH"
}

& pwsh -NoProfile -File "$PSScriptRoot/build-native.ps1"
if ($LASTEXITCODE) { throw 'Native build failed.' }
& cmake -S "$PSScriptRoot/../windows" -B $BuildDirectory -G $Generator "-DCMAKE_PREFIX_PATH=$QtPath" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& cmake --build $BuildDirectory --config Release --parallel
if ($LASTEXITCODE) { throw 'Windows build failed.' }
& ctest --test-dir $BuildDirectory -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Windows tests failed.' }
