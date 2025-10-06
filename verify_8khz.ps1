# 8 kHz Polling Rate Verification Script
# This script helps verify if your keyboard is actually running at 8 kHz

Write-Host ""
Write-Host "========================================================================"
Write-Host "  8 kHz USB Polling Verification"
Write-Host "========================================================================"
Write-Host ""

# Check if device is connected
Write-Host "[1/3] Checking for OpenGrader device..." -ForegroundColor Cyan
Write-Host ""

$devices = Get-PnpDevice | Where-Object { 
    $_.FriendlyName -like "*OpenGrader*" -or 
    $_.FriendlyName -like "*HID*Keyboard*" 
}

if ($devices) {
    Write-Host "Device(s) found:" -ForegroundColor Green
    $devices | ForEach-Object {
        Write-Host "  ✓ $($_.FriendlyName)" -ForegroundColor Cyan
        Write-Host "    Status: $($_.Status)" -ForegroundColor Gray
        Write-Host "    Instance ID: $($_.InstanceId)" -ForegroundColor Gray
    }
    Write-Host ""
} else {
    Write-Host "No OpenGrader device found!" -ForegroundColor Red
    Write-Host "Make sure your keyboard is connected and firmware is running." -ForegroundColor Yellow
    Write-Host ""
}

# Check USB descriptors (requires USBView or similar)
Write-Host "[2/3] USB Descriptor Information" -ForegroundColor Cyan
Write-Host ""
Write-Host "To verify 8 kHz polling rate, you need to check the USB descriptors:"
Write-Host ""
Write-Host "Method 1: Using USBView (Recommended)" -ForegroundColor Yellow
Write-Host "  1. Download USBView from Microsoft:" -ForegroundColor Gray
Write-Host "     https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/usbview" -ForegroundColor Gray
Write-Host "  2. Run USBView.exe" -ForegroundColor Gray
Write-Host "  3. Find your OpenGrader device" -ForegroundColor Gray
Write-Host "  4. Look for HID endpoints" -ForegroundColor Gray
Write-Host "  5. Check 'bInterval' value:" -ForegroundColor Gray
Write-Host "     - bInterval = 1 → 8 kHz (0.125 ms) ✓ CORRECT" -ForegroundColor Green
Write-Host "     - bInterval = 10 → 1 kHz (1 ms) ✗ OLD CONFIG" -ForegroundColor Red
Write-Host ""

Write-Host "Method 2: Using Device Manager" -ForegroundColor Yellow
Write-Host "  1. Open Device Manager" -ForegroundColor Gray
Write-Host "  2. Find your keyboard under 'Human Interface Devices'" -ForegroundColor Gray
Write-Host "  3. Right-click → Properties → Details" -ForegroundColor Gray
Write-Host "  4. Select 'Device instance path' from dropdown" -ForegroundColor Gray
Write-Host ""

Write-Host "Method 3: Using PowerShell (Limited info)" -ForegroundColor Yellow
Write-Host "  Running now..." -ForegroundColor Gray
Write-Host ""

# Try to get USB device info
try {
    $usbDevices = Get-PnpDeviceProperty -InstanceId "USB\*" -ErrorAction SilentlyContinue | 
                  Where-Object { $_.Data -like "*OpenGrader*" -or $_.Data -like "*Keyboard*" }
    
    if ($usbDevices) {
        Write-Host "  Found USB device properties:" -ForegroundColor Green
        $usbDevices | Select-Object -First 5 | ForEach-Object {
            Write-Host "  $($_.KeyName): $($_.Data)" -ForegroundColor Cyan
        }
    }
} catch {
    Write-Host "  Could not retrieve detailed USB info via PowerShell" -ForegroundColor Yellow
}

Write-Host ""

# Check firmware configuration
Write-Host "[3/3] Checking Firmware Configuration" -ForegroundColor Cyan
Write-Host ""

$configFile = "Core\Inc\tusb_config.h"
if (Test-Path $configFile) {
    $content = Get-Content $configFile
    $pollInterval = $content | Select-String "CFG_TUD_HID_POLL_INTERVAL"
    
    if ($pollInterval) {
        Write-Host "Source code configuration:" -ForegroundColor Green
        Write-Host $pollInterval -ForegroundColor Cyan
        Write-Host ""
        
        if ($pollInterval -match "CFG_TUD_HID_POLL_INTERVAL\s+1") {
            Write-Host "✓ Configuration is CORRECT (set to 1 = 8 kHz)" -ForegroundColor Green
        } elseif ($pollInterval -match "CFG_TUD_HID_POLL_INTERVAL\s+10") {
            Write-Host "✗ Configuration is OLD (set to 10 = 1 kHz)" -ForegroundColor Red
            Write-Host "  This should have been changed to 1 for 8 kHz" -ForegroundColor Yellow
        } else {
            Write-Host "? Configuration value unclear" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "Could not find tusb_config.h" -ForegroundColor Yellow
}

Write-Host ""

# Check if firmware needs reflashing
Write-Host "========================================================================"
Write-Host "  Action Required"
Write-Host "========================================================================"
Write-Host ""

$elfPath = "build\Debug\TINYUSBTEST.elf"
if (Test-Path $elfPath) {
    $buildTime = (Get-Item $elfPath).LastWriteTime
    Write-Host "Firmware file: $elfPath" -ForegroundColor Cyan
    Write-Host "Last built: $buildTime" -ForegroundColor Gray
    Write-Host ""
    
    $minutesAgo = [math]::Round(((Get-Date) - $buildTime).TotalMinutes, 1)
    
    if ($minutesAgo -lt 5) {
        Write-Host "✓ Firmware is recent (built $minutesAgo minutes ago)" -ForegroundColor Green
    } else {
        Write-Host "⚠ Firmware is older (built $minutesAgo minutes ago)" -ForegroundColor Yellow
        Write-Host "  Consider rebuilding if you changed configuration" -ForegroundColor Gray
    }
} else {
    Write-Host "✗ Firmware file not found at: $elfPath" -ForegroundColor Red
    Write-Host "  You need to build the firmware first!" -ForegroundColor Yellow
}

Write-Host ""

# Recommendations
Write-Host "========================================================================"
Write-Host "  Next Steps"
Write-Host "========================================================================"
Write-Host ""

Write-Host "If USB descriptors show bInterval = 10 (1 kHz):" -ForegroundColor Yellow
Write-Host "  1. You may have old firmware flashed" -ForegroundColor White
Write-Host "  2. Rebuild: cmake --build build" -ForegroundColor White
Write-Host "  3. Reflash: .\flash_usb.bat" -ForegroundColor White
Write-Host ""

Write-Host "If USB descriptors show bInterval = 1 (8 kHz):" -ForegroundColor Green
Write-Host "  ✓ Your keyboard IS running at 8 kHz!" -ForegroundColor White
Write-Host "  The '1k' you're seeing might be matrix scan rate, not USB polling" -ForegroundColor White
Write-Host ""

Write-Host "Matrix Scan Rate vs USB Polling Rate:" -ForegroundColor Cyan
Write-Host "  • Matrix Scan Rate: How often firmware checks key switches (typically 1 kHz)" -ForegroundColor Gray
Write-Host "  • USB Polling Rate: How often PC requests data (now 8 kHz)" -ForegroundColor Gray
Write-Host "  • Both are independent! 1 kHz scan + 8 kHz USB is normal and correct" -ForegroundColor Gray
Write-Host ""

Write-Host "Testing Polling Rate:" -ForegroundColor Cyan
Write-Host "  1. Use NVIDIA Reflex Latency Analyzer (hardware test)" -ForegroundColor Gray
Write-Host "  2. Use Human Benchmark: https://humanbenchmark.com/tests/reactiontime" -ForegroundColor Gray
Write-Host "  3. Use specialist software like 'Mouse Tester' or 'Input Lag Test'" -ForegroundColor Gray
Write-Host ""

Read-Host "Press Enter to exit"
