# miquoverview

`miquoverview` is a GPU-accelerated live workspace expose and overview plugin for `miquland`.

## Features
- **GPU-Accelerated Live Window Buffers**: Uses wlroots / SceneFX scene graph scaling (`wlr_scene_buffer_set_dest_size`) to scale live running surfaces without client-side resize events. Videos keep playing and terminals keep scrolling live in the overview!
- **Interactive Workspace Cards**: Visually displays all active and adjacent workspaces in an adaptive grid (2x2, 3x2, etc.) with workspace status, window counts, and active highlighting.
- **Direct Window & Workspace Activation**: Click any window to switch to its workspace and focus it instantly. Click any workspace card to switch to that workspace.
- **Keyboard Navigation**: Press `1`-`9` to switch workspaces, arrow keys to navigate, `Enter` to select, or `Escape` to close.
- **Hyprland-Style C++ Plugin Architecture**: Implemented as a dynamic C++ shared library (`libmiquoverview.so`) loaded via `miquland`'s plugin system.

## Configuration
In `~/.config/miquland/miquland.conf`:

```ini
# Load plugin
plugin = /usr/lib/miquland/plugins/libmiquoverview.so

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
