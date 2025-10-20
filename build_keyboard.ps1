# Build script for different keyboard configurations
# Usage: .\build_keyboard.ps1 [keyboard_name] [clean]
# Example: .\build_keyboard.ps1 onekey
# Example: .\build_keyboard.ps1 standard clean

param(
    [string]$Keyboard = "standard",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

Write-Host "Building for keyboard: $Keyboard" -ForegroundColor Cyan

# Check if keyboard directory exists
$keyboardPath = "keyboards\$Keyboard"
if (-not (Test-Path $keyboardPath)) {
    Write-Host "Error: Keyboard '$Keyboard' not found in keyboards directory" -ForegroundColor Red
    Write-Host "Available keyboards:" -ForegroundColor Yellow
    Get-ChildItem keyboards -Directory | ForEach-Object { Write-Host "  - $($_.Name)" }
    exit 1
}

# Clean build if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force build
    }
}

# Configure CMake with the selected keyboard
Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake -B build -DKEYBOARD=$Keyboard

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "Building firmware..." -ForegroundColor Yellow
cmake --build build

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`nBuild completed successfully for keyboard: $Keyboard" -ForegroundColor Green
Write-Host "Binary location: build\Debug\TINYUSBTEST.elf" -ForegroundColor Cyan
