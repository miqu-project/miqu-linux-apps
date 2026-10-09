# miqubar

Independent Wayland status bar and Windows-style taskbar for Miquland built natively with `miqutoolkit`.

## Features
- **Start Menu Button**: Spawns application runner (`miqulauncher` by default).
- **Compact Workspace Button**: Displays workspace glyph with an active workspace badge (`[ ⊞ 1 ]`).
  - Scroll mouse wheel over the button to instantly cycle workspaces.
  - Hover over or click to reveal an interactive workspace flyout.
- **Running Window Taskbar**: Tracks open windows via `zwlr_foreign_toplevel_manager_v1`.
  - Application icon, window title, and active accent underline indicator.
  - Left-click to focus/minimize; middle-click to close; right-click for window context menu.
- **Hardware Telemetry Modules**: Real-time CPU, RAM, and Battery percentage indicators.
- **Audio Control**: Volume indicator; scroll to adjust volume; click to open Quick Settings flyout.
- **Windows-Style Stacked Clock**: Time and Date display; click to open interactive Calendar flyout.
- **"Show Desktop" Peek Button**: Narrow button on the far right edge to minimize/restore all windows.
- **Layer Shell Integration**: Anchored to bottom or top with an exclusive zone so windows tile seamlessly.

## Configuration
Generate default user configuration:
```bash
miqubar --init-config
```
Configuration file is located at `~/.config/miqubar/miqubar.conf`.

## Build & Install
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```
