# ========================================================================
#  OpenGRADER USB Flash Script (PowerShell)
#  Automatically builds and flashes firmware via USB DFU
# ========================================================================

Write-Host ""
Write-Host "========================================================================"
Write-Host "  OpenGRADER USB Flash Script"
Write-Host "========================================================================"
Write-Host ""

# Check if STM32CubeProgrammer is installed
$programmer = Get-Command STM32_Programmer_CLI -ErrorAction SilentlyContinue

if (-not $programmer) {
    Write-Host "[ERROR] STM32_Programmer_CLI not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install STM32CubeProgrammer and add to PATH:"
    Write-Host "  Download: https://www.st.com/en/development-tools/stm32cubeprog.html"
    Write-Host ""
    Write-Host "Or add manually to PATH:"
    Write-Host '  $env:Path += ";C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin"'
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# Build firmware
Write-Host "[1/4] Building firmware..." -ForegroundColor Cyan
Write-Host ""

$buildResult = & cmake --build build --config Debug 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    Write-Host "Please check CMake configuration and build errors above."
    Write-Host ""
    Write-Host $buildResult
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "[SUCCESS] Build completed!" -ForegroundColor Green
Write-Host ""

# Check if firmware exists
$firmwarePath = "build\TINYUSBTEST.elf"
if (-not (Test-Path $firmwarePath)) {
    Write-Host "[ERROR] Firmware file not found: $firmwarePath" -ForegroundColor Red
    Write-Host "Please check build output above."
    Read-Host "Press Enter to exit"
    exit 1
}

$firmwareSize = (Get-Item $firmwarePath).Length
Write-Host "Firmware size: $([math]::Round($firmwareSize / 1KB, 2)) KB"
Write-Host ""

# Prompt to enter DFU mode
Write-Host "========================================================================"
Write-Host "[2/4] Enter DFU Mode" -ForegroundColor Cyan
Write-Host "========================================================================"
Write-Host ""
Write-Host "Please prepare your STM32G474 board:"
Write-Host "  1. DISCONNECT USB cable (if connected)" -ForegroundColor Yellow
Write-Host "  2. HOLD the BOOT0 button" -ForegroundColor Yellow
Write-Host "  3. CONNECT USB cable while holding BOOT0" -ForegroundColor Yellow
Write-Host "  4. RELEASE the BOOT0 button" -ForegroundColor Yellow
Write-Host ""
Write-Host "The device should appear in Device Manager as 'STM32 BOOTLOADER'"
Write-Host ""
Read-Host "Press Enter when ready"

# Check if device is in DFU mode
Write-Host ""
Write-Host "[3/4] Checking for DFU device..." -ForegroundColor Cyan
Write-Host ""

$deviceList = & STM32_Programmer_CLI -l 2>&1
$dfuFound = $deviceList -match "USB"

if (-not $dfuFound) {
    Write-Host "[WARNING] Could not detect DFU device!" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Possible issues:"
    Write-Host "  - BOOT0 not pressed during USB connection"
    Write-Host "  - USB drivers not installed (use Zadig to install WinUSB driver)"
    Write-Host "  - USB cable is charge-only (use a data cable)"
    Write-Host ""
    
    $continue = Read-Host "Do you want to continue anyway? (y/n)"
    if ($continue -ne "y") {
        Write-Host ""
        Write-Host "Flash cancelled."
        Read-Host "Press Enter to exit"
        exit 1
    }
} else {
    Write-Host "[SUCCESS] DFU device detected!" -ForegroundColor Green
    Write-Host $deviceList | Where-Object { $_ -match "USB" }
}

# Flash firmware
Write-Host ""
Write-Host "[4/4] Flashing firmware..." -ForegroundColor Cyan
Write-Host ""
Write-Host "Target: $firmwarePath"
Write-Host "Address: 0x08000000"
Write-Host ""

$flashResult = & STM32_Programmer_CLI -c port=usb1 -d $firmwarePath -v -hardRst -rst 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================================================"
    Write-Host "  [SUCCESS] Firmware flashed successfully!" -ForegroundColor Green
    Write-Host "========================================================================"
    Write-Host ""
    Write-Host "Your device should now be running the new firmware."
    Write-Host ""
    Write-Host "The keyboard should enumerate as:"
    Write-Host "  ✓ USB HID Keyboard (8 kHz polling)" -ForegroundColor Green
    Write-Host "  ✓ USB HID Mouse" -ForegroundColor Green
    Write-Host "  ✓ USB MIDI Device" -ForegroundColor Green
    Write-Host "  ✓ USB CDC Serial Port" -ForegroundColor Green
    Write-Host "  ✓ USB Config Interface" -ForegroundColor Green
    Write-Host ""
    Write-Host "If the device is still in bootloader mode, press the RESET button."
    Write-Host ""
    
    # Try to detect the device
    Write-Host "Detecting device..."
    Start-Sleep -Seconds 2
    $devices = Get-PnpDevice | Where-Object { $_.FriendlyName -like "*OpenGrader*" -or $_.FriendlyName -like "*HID*Keyboard*" }
    if ($devices) {
        Write-Host ""
        Write-Host "Device(s) found:" -ForegroundColor Green
        $devices | ForEach-Object {
            Write-Host "  - $($_.FriendlyName)" -ForegroundColor Cyan
        }
    }
    
} else {
    Write-Host ""
    Write-Host "========================================================================"
    Write-Host "  [ERROR] Flash failed!" -ForegroundColor Red
    Write-Host "========================================================================"
    Write-Host ""
    Write-Host "Flash output:"
    Write-Host $flashResult
    Write-Host ""
    Write-Host "Common issues:"
    Write-Host "  1. Device not in DFU mode"
    Write-Host "     - Solution: Hold BOOT0 before connecting USB"
    Write-Host ""
    Write-Host "  2. USB drivers not installed"
    Write-Host "     - Solution: Use Zadig to install WinUSB driver"
    Write-Host "     - Download: https://zadig.akeo.ie/"
    Write-Host ""
    Write-Host "  3. Wrong USB port"
    Write-Host "     - Solution: Try a different USB port (USB 2.0, not hub)"
    Write-Host ""
    Write-Host "  4. Permission denied"
    Write-Host "     - Solution: Run PowerShell as Administrator"
    Write-Host ""
    Write-Host "For detailed troubleshooting, see USB_FLASHING_GUIDE.md"
    Write-Host ""
}

Write-Host ""
Read-Host "Press Enter to exit"
