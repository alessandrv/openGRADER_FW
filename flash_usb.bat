@echo off
REM ========================================================================
REM  OpenGRADER USB Flash Script
REM  Automatically builds and flashes firmware via USB DFU
REM ========================================================================

echo.
echo ========================================================================
echo   OpenGRADER USB Flash Script
echo ========================================================================
echo.

REM Check if STM32CubeProgrammer is installed
where STM32_Programmer_CLI >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] STM32_Programmer_CLI not found!
    echo.
    echo Please install STM32CubeProgrammer and add to PATH, or use full path:
    echo   "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI"
    echo.
    echo Download: https://www.st.com/en/development-tools/stm32cubeprog.html
    echo.
    pause
    exit /b 1
)

REM Build firmware
echo [1/4] Building firmware...
echo.
cmake --build build --config Debug
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed!
    echo Please check CMake configuration and build errors above.
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build completed!
echo.

REM Check if firmware exists
if not exist "build\TINYUSBTEST.elf" (
    echo [ERROR] Firmware file not found: build\TINYUSBTEST.elf
    echo Please check build output above.
    pause
    exit /b 1
)

REM Prompt to enter DFU mode
echo ========================================================================
echo [2/4] Enter DFU Mode
echo ========================================================================
echo.
echo Please prepare your STM32G474 board:
echo   1. DISCONNECT USB cable (if connected)
echo   2. HOLD the BOOT0 button
echo   3. CONNECT USB cable while holding BOOT0
echo   4. RELEASE the BOOT0 button
echo.
echo The device should appear in Device Manager as "STM32 BOOTLOADER"
echo.
pause

REM Check if device is in DFU mode
echo.
echo [3/4] Checking for DFU device...
echo.
STM32_Programmer_CLI -l >nul 2>nul
if %errorlevel% neq 0 (
    echo [WARNING] Could not detect DFU device!
    echo.
    echo Possible issues:
    echo   - BOOT0 not pressed during USB connection
    echo   - USB drivers not installed (use Zadig to install WinUSB driver)
    echo   - USB cable is charge-only (use a data cable)
    echo.
    echo Do you want to continue anyway? (y/n)
    choice /c yn /n
    if errorlevel 2 (
        echo.
        echo Flash cancelled.
        pause
        exit /b 1
    )
)

REM Flash firmware
echo.
echo [4/4] Flashing firmware...
echo.
echo Target: build\TINYUSBTEST.elf
echo Address: 0x08000000
echo.

STM32_Programmer_CLI -c port=usb1 -d build\TINYUSBTEST.elf -v -hardRst -rst

if %errorlevel% equ 0 (
    echo.
    echo ========================================================================
    echo   [SUCCESS] Firmware flashed successfully!
    echo ========================================================================
    echo.
    echo Your device should now be running the new firmware.
    echo.
    echo The keyboard should enumerate as:
    echo   - USB HID Keyboard (8 kHz polling)
    echo   - USB HID Mouse
    echo   - USB MIDI Device
    echo   - USB CDC Serial Port
    echo   - USB Config Interface
    echo.
    echo If the device is still in bootloader mode, press the RESET button.
    echo.
) else (
    echo.
    echo ========================================================================
    echo   [ERROR] Flash failed!
    echo ========================================================================
    echo.
    echo Common issues:
    echo   1. Device not in DFU mode
    echo      - Solution: Hold BOOT0 before connecting USB
    echo.
    echo   2. USB drivers not installed
    echo      - Solution: Use Zadig to install WinUSB driver
    echo      - Download: https://zadig.akeo.ie/
    echo.
    echo   3. Wrong USB port
    echo      - Solution: Try a different USB port (USB 2.0, not hub)
    echo.
    echo   4. Permission denied
    echo      - Solution: Run this script as Administrator
    echo.
    echo For detailed troubleshooting, see USB_FLASHING_GUIDE.md
    echo.
)

pause
