<#
.SYNOPSIS
    One-command developer bootstrap for the Game Engine for Teaching.
.DESCRIPTION
    Automates: vcpkg, Python deps, CMake configure+build, asset cook, smoke test.

    Prerequisites:
      - Visual Studio 2022 (Desktop C++ workload)
      - Python 3.9+
      - Git in PATH

    Usage:
        .\scripts\bootstrap.ps1
        .\scripts\bootstrap.ps1 -VcpkgRoot C:\my\vcpkg -Preset windows-ninja-debug-engine-only
#>

param(
    [string]$VcpkgRoot = "C:\vcpkg",
    [string]$Preset    = "windows-ninja-debug-engine-only"
)

# TEACHING NOTE — PowerShell strict mode
# Set-StrictMode catches common bugs:
#   - References to undefined variables
#   - Property access on null objects
# $ErrorActionPreference = "Stop" makes any failed command terminate the script
# immediately rather than silently continuing.
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path $PSScriptRoot -Parent

Write-Host "[bootstrap] Repo root: $RepoRoot" -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# 1. Bootstrap vcpkg if not present
# ---------------------------------------------------------------------------
# TEACHING NOTE — vcpkg is Microsoft's C++ package manager.
# It downloads, builds, and installs C++ libraries from source so
# you do not need to manually compile nlohmann-json, imgui, joltphysics etc.
# The bootstrap-vcpkg.bat script builds the vcpkg.exe binary itself.
# ---------------------------------------------------------------------------
if (-not (Test-Path "$VcpkgRoot\vcpkg.exe")) {
    Write-Host "[bootstrap] vcpkg not found at $VcpkgRoot — cloning and bootstrapping..." -ForegroundColor Yellow
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot --depth 1
    & "$VcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
} else {
    Write-Host "[bootstrap] vcpkg found at $VcpkgRoot" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# 2. Python dependencies
# ---------------------------------------------------------------------------
# TEACHING NOTE — requirements-dev.txt lists all Python packages needed
# by the authoring tools (Pillow for texture cooking, pytest for tests, etc.).
# pip install -r installs every package in the file.
# ---------------------------------------------------------------------------
Write-Host "[bootstrap] Installing Python dependencies..." -ForegroundColor Cyan
$ReqFile = Join-Path $RepoRoot "requirements-dev.txt"
if (Test-Path $ReqFile) {
    python -m pip install -r $ReqFile --quiet
} else {
    Write-Warning "[bootstrap] requirements-dev.txt not found — skipping Python deps."
}

# ---------------------------------------------------------------------------
# 3. CMake configure
# ---------------------------------------------------------------------------
# TEACHING NOTE — cmake --preset reads configuration from CMakePresets.json.
# The engine-only preset builds engine_sandbox and cook WITHOUT the Qt editor
# so no Qt installation is required.
# ---------------------------------------------------------------------------
Write-Host "[bootstrap] Configuring CMake preset: $Preset..." -ForegroundColor Cyan
Push-Location $RepoRoot
try {
    $ToolchainArg = "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
    cmake --preset $Preset $ToolchainArg
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }
} finally {
    Pop-Location
}

# ---------------------------------------------------------------------------
# 4. Build
# ---------------------------------------------------------------------------
Write-Host "[bootstrap] Building engine_sandbox, cook, pak..." -ForegroundColor Cyan
Push-Location $RepoRoot
try {
    cmake --build --preset $Preset --target engine_sandbox cook pak
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)" }
} finally {
    Pop-Location
}

# ---------------------------------------------------------------------------
# 5. Cook vertical slice assets
# ---------------------------------------------------------------------------
# TEACHING NOTE — cook_assets.py converts raw source assets (PNG, WAV, JSON)
# into engine-ready cooked formats (DDS textures, packed banks, binary clips).
# Run it whenever you change files under Content/.
# ---------------------------------------------------------------------------
Write-Host "[bootstrap] Cooking vertical slice assets..." -ForegroundColor Cyan
Push-Location (Join-Path $RepoRoot "samples\vertical_slice_project")
try {
    python cook_assets.py
    if ($LASTEXITCODE -ne 0) { throw "cook_assets.py failed (exit $LASTEXITCODE)" }
} finally {
    Pop-Location
}

# ---------------------------------------------------------------------------
# 6. Smoke tests
# ---------------------------------------------------------------------------
# TEACHING NOTE — The headless mode (--headless) skips the render loop and
# runs acceptance tests using D3D11 WARP (CPU software rasteriser).
# This works on any machine — no GPU or display required.
# ---------------------------------------------------------------------------
$BuildDir  = Join-Path $RepoRoot "build\$Preset"

# TEACHING NOTE — Constructed path vs. recursive search
# We construct the expected path directly from the known preset output structure
# (Ninja Multi-Config places binaries in build/<preset>/Debug/ for Debug config
# and build/<preset>/ for single-config generators).  Recursive Search-Item is
# fragile when multiple build presets exist under the same root directory.
$ExeDir    = $BuildDir
# Try Debug sub-dir first (Ninja Multi-Config); fall back to preset root.
if (Test-Path (Join-Path $ExeDir "Debug\engine_sandbox.exe")) {
    $ExePath = Join-Path $ExeDir "Debug\engine_sandbox.exe"
} elseif (Test-Path (Join-Path $ExeDir "engine_sandbox.exe")) {
    $ExePath = Join-Path $ExeDir "engine_sandbox.exe"
} else {
    throw "[bootstrap] engine_sandbox.exe not found under build\$Preset"
}

Write-Host "[bootstrap] Smoke test: --headless..." -ForegroundColor Cyan
& $ExePath --headless
if ($LASTEXITCODE -ne 0) { throw "Headless smoke test failed (exit $LASTEXITCODE)" }

Write-Host "[bootstrap] Smoke test: --headless --scene dynamic_sky..." -ForegroundColor Cyan
& $ExePath --headless --scene dynamic_sky
if ($LASTEXITCODE -ne 0) { throw "dynamic_sky scene failed (exit $LASTEXITCODE)" }

Write-Host ""
Write-Host "[bootstrap] SUCCESS — engine is ready!" -ForegroundColor Green
Write-Host "  Executable : $ExePath"
Write-Host "  Try        : $ExePath --headless --scene pbr_mesh"
Write-Host "  Or open    : $ExePath --scene game"
