# VS Code Task Quick Reference

## Task Naming Convention

All keyboard-specific tasks now follow this pattern:
```
[Action]: [Keyboard Name] ([Connection Type])
```

Examples:
- `Build: Standard Keyboard` - Build only
- `Flash: OneKey Keyboard (USB)` - Flash only via USB
- `Build + Flash: Standard Keyboard (USB)` - Build then flash via USB

## Quick Task List

### 🔨 Build Tasks
```
Build: Standard Keyboard
Build: OneKey Keyboard
Clean Build: Standard Keyboard
Clean Build: OneKey Keyboard
```

### 📲 Flash Tasks (USB - Recommended)
```
Flash: Standard Keyboard (USB)
Flash: OneKey Keyboard (USB)
Build + Flash: Standard Keyboard (USB)
Build + Flash: OneKey Keyboard (USB)
```

### 🔌 Flash Tasks (SWD - Debug Mode)
```
Build + Flash: Standard Keyboard (SWD)
Build + Flash: OneKey Keyboard (SWD)
```

### ⚙️ Configuration Tasks
```
Configure: Standard Keyboard
Configure: OneKey Keyboard
```

## Most Common Workflows

### Daily Development (USB Flash)
1. Edit code
2. Press `Ctrl+Shift+B`
3. Select `Build + Flash: [Your Keyboard] (USB)`
4. Done!

### Quick Test Another Keyboard
1. `Ctrl+Shift+P` → `Tasks: Run Task`
2. Select `Build + Flash: [Other Keyboard] (USB)`
3. New keyboard firmware flashes automatically

### Debug with ST-Link
1. Connect ST-Link probe
2. `Ctrl+Shift+P` → `Tasks: Run Task`
3. Select `Build + Flash: [Your Keyboard] (SWD)`
4. Debug as normal

## Connection Types

### USB (port=usb1)
- ✅ No external debugger needed
- ✅ Uses built-in USB DFU bootloader
- ✅ Faster for production use
- ⚠️ Requires device in DFU mode
- Command: `STM32_Programmer_CLI -c port=usb1 -d <elf> -v --start`

### SWD (port=swd)
- ✅ Works even if firmware is broken
- ✅ Can debug with breakpoints
- ✅ More reliable for development
- ⚠️ Requires ST-Link debugger hardware
- Command: `STM32_Programmer_CLI --connect port=swd --download <elf> -hardRst -rst --start`

## Keyboard Names in Tasks

Each keyboard's name appears in the task label:
- **Standard Keyboard** = 5×7 matrix, 25 encoders
- **OneKey Keyboard** = 1×1 matrix, 0 encoders

This makes it crystal clear which keyboard you're building/flashing.

## Pro Tips

💡 **Pin Task to Top**: Right-click task → "Pin" to keep it at top of list

💡 **Keyboard Shortcut**: Assign `Ctrl+Shift+F` to your favorite flash task:
   - File → Preferences → Keyboard Shortcuts
   - Search for task name
   - Assign key binding

💡 **Terminal Output**: Flash tasks show full output with `-v` (verbose) flag

💡 **Auto-Start**: Tasks include `--start` flag to automatically run firmware after flash
