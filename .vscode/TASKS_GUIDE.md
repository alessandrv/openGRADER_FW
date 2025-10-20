# VS Code Tasks for Multi-Keyboard Builds

## Available Tasks

You can run these tasks via:
- **Command Palette**: `Ctrl+Shift+P` → `Tasks: Run Task`
- **Terminal Menu**: `Terminal` → `Run Task...`
- **Keyboard Shortcut**: `Ctrl+Shift+B` (shows build tasks)

### Configuration Tasks

These configure the build system for a specific keyboard:

- **Configure: Standard Keyboard** - Set up build for the 5×7 standard keyboard
- **Configure: OneKey Keyboard** - Set up build for the 1×1 onekey keyboard

### Build Tasks

These build the firmware (after configuration):

- **Build: Standard Keyboard** - Configure + build standard keyboard
- **Build: OneKey Keyboard** - Configure + build onekey keyboard

### Clean Build Tasks

These do a complete clean rebuild:

- **Clean Build: Standard Keyboard** - Clean + rebuild standard keyboard
- **Clean Build: OneKey Keyboard** - Clean + rebuild onekey keyboard

### Flash Tasks

These build and flash the firmware to your device:

#### USB Flashing (Recommended)
- **Build + Flash: Standard Keyboard (USB)** - Build standard + flash via USB DFU
- **Build + Flash: OneKey Keyboard (USB)** - Build onekey + flash via USB DFU
- **Flash: Standard Keyboard (USB)** - Flash pre-built standard binary
- **Flash: OneKey Keyboard (USB)** - Flash pre-built onekey binary

#### SWD Flashing (Debug)
- **Build + Flash: Standard Keyboard (SWD)** - Build standard + flash via ST-Link
- **Build + Flash: OneKey Keyboard (SWD)** - Build onekey + flash via ST-Link

### Utility Tasks

- **CubeProg: Flash project (USB)** - Flash current binary via USB DFU
- **CubeProg: Flash project (SWD)** - Flash current binary via ST-Link
- **CubeProg: List all available communication interfaces** - List available devices
- **Build + Flash (USB)** - Build current config + flash via USB
- **Build + Flash (SWD)** - Build current config + flash via SWD
- **CMake: clean rebuild** - Clean rebuild current configuration

## Quick Workflow

### First Time / Switching Keyboards

1. Run: `Build: Standard Keyboard` or `Build: OneKey Keyboard`
   - This configures CMake and builds everything

### Incremental Builds

After making code changes, just run:
- `CMake: clean rebuild` (uses current keyboard configuration)
- Or use the CMake extension's build button

### Flash Firmware

#### Via USB (Recommended - No Debugger Needed)
1. Put device in DFU mode (if needed)
2. Run: `Build + Flash: Standard Keyboard (USB)` or `Build + Flash: OneKey Keyboard (USB)`
   - This builds and automatically flashes via USB

#### Via ST-Link (For Debugging)
1. Connect ST-Link debugger
2. Run: `Build + Flash: Standard Keyboard (SWD)` or `Build + Flash: OneKey Keyboard (SWD)`
   - This builds and automatically flashes via SWD

## Tips

- **Default Build**: Press `Ctrl+Shift+B` to see all build tasks
- **Task History**: Recently used tasks appear at the top
- **CMake Extension**: The CMake Tools extension works alongside these tasks
- **Configuration Persists**: Once configured, you can use regular CMake commands

## Keyboard-Specific Settings

Each keyboard task automatically:
- Sets the correct `KEYBOARD` CMake variable
- Uses the Debug preset
- Configures the proper toolchain
- Links the keyboard-specific `keymap.c` file

## Examples

### Switch from Standard to OneKey

1. `Ctrl+Shift+P` → `Tasks: Run Task`
2. Select `Build: OneKey Keyboard`
3. Wait for build to complete
4. Binary is at: `build/Debug/TINYUSBTEST.elf`

### Flash OneKey to Device (USB)

1. Put device in DFU mode (if needed)
2. `Ctrl+Shift+P` → `Tasks: Run Task`
3. Select `Build + Flash: OneKey Keyboard (USB)`
4. Firmware automatically flashes and starts

### Flash Standard to Device (USB)

1. Put device in DFU mode (if needed)
2. `Ctrl+Shift+P` → `Tasks: Run Task`
3. Select `Build + Flash: Standard Keyboard (USB)`
4. Firmware automatically flashes and starts

### Quick Rebuild Current Config

1. `Ctrl+Shift+P` → `Tasks: Run Task`
2. Select `CMake: clean rebuild`
3. Uses whatever keyboard was last configured
