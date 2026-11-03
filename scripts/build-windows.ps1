# Builds Clipline on Windows with Visual Studio 2022 and runs the tests.
#   .\scripts\build-windows.ps1                 Release build + tests
#   .\scripts\build-windows.ps1 -SkipTests
#   .\scripts\build-windows.ps1 -Install        also run the built installer (asks for administrator rights)
param(
    [string]$Config = "Release",
    [string]$BuildDir = "build",
    [switch]$SkipTests,
    [switch]$Install
)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

foreach ($tool in "cmake", "git") {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool was not found. Install Visual Studio 2022 (Desktop development with C++) and Git, then run this script from a 'Developer PowerShell for VS 2022'."
    }
}

cmake -B $BuildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$plugin    = Join-Path $BuildDir "Clipline_artefacts\$Config\VST3\Clipline.vst3"
$installer = Get-ChildItem (Join-Path $BuildDir "Installer\Windows\$Config") -Filter "*-Setup.exe" | Select-Object -First 1

if (-not $SkipTests) {
    & (Join-Path $BuildDir "Tests\ClipperTests_artefacts\$Config\ClipperTests.exe")
    if ($LASTEXITCODE -ne 0) { throw "Unit tests failed" }
    & (Join-Path $BuildDir "Tests\ClipperHostCheck_artefacts\$Config\ClipperHostCheck.exe") $plugin
    if ($LASTEXITCODE -ne 0) { throw "VST3 host check failed" }
}

Write-Host ""
Write-Host "Plugin:    $plugin"
Write-Host "Installer: $($installer.FullName)"

if ($Install) { Start-Process -FilePath $installer.FullName -Wait }
