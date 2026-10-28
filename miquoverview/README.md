# miquoverview

`miquoverview` is a GPU-accelerated live workspace expose and overview plugin for `miquland`.

## Features
- **Full-Screen Responsive Overview**: Fills the usable screen space while respecting exclusive reservations set by desktop components such as `miqubar`.
- **Configurable Edge Margins**: Preserves a comfortable, polished perimeter border around the workspace display (configurable in `miquoverview.conf`).
- **Screen-Aspect Workspace Geometry**: Accurately scales the entire workspace into a prominent central rectangle matching the exact monitor aspect ratio.
- **Live Hardware-Accelerated Rendering**: Windows inside the active workspace are placed and scaled live with zero frame-dropping using wlroots hardware scene nodes (`v->set_overview_scaled`).
- **Workspace Navigation & Pager**: Interactive top workspace pill bar showing open workspaces, window counts, and active selection.
- **Direct Window & Workspace Activation**: Click any window to switch to that workspace and focus it immediately; click empty space to activate that workspace; click outside the workspace to dismiss.
- **Complete Keyboard Navigation**: Use `←`/`→` (or `h`/`l`) to switch between workspaces, `↑`/`↓` (or `k`/`j` / `Tab`) to cycle windows, `1`-`9` for instant workspace jumps, `Enter` to activate, and `Escape` to close.
- **Mouse Wheel Scrolling**: Switch across workspaces with natural vertical or horizontal scroll wheel gestures.
- **Uniform Two-Tier Configuration**: Supports standalone user configurations and live dynamic reload with `miqutheme`.

## Configuration & Theming
`miquoverview` follows the uniform Miqu ecosystem Two-Tier Architecture:
- **Tier 1 (System Baseline)**: `/usr/share/miquoverview/miquoverview.conf`
- **Tier 2 (User Standalone)**: `~/.config/miquoverview/miquoverview.conf`

It includes a self-contained baseline fallback theme and optionally supports decoupled dynamic theming via:
```ini
source = ~/.config/miqutheme/current/miqutoolkit.conf
```
Config changes and theme updates are watched via `inotify` and hot-reloaded automatically with 0% idle CPU.

In `~/.config/miquland/miquland.conf`:
```ini
# Load plugin
plugins {
    load = /usr/lib/miquland/plugins/libmiquoverview.so
}

# Keybindings
bind = SUPER, Tab, overview_toggle
bind = SUPER, o, overview_toggle
```

## Building & Installation

### Local Build (No root)
```bash
./make.sh
```

### System-wide Installation (/usr)
```bash
sudo ./make.sh
```
