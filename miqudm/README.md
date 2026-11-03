# miqudm - Wayland Display Manager

`miqudm` is a lightweight, pure Wayland Display Manager and Session Greeter designed for the `miqu` desktop ecosystem and modern Linux systems.

---

## Architecture

`miqudm` is strictly decomposed into two decoupled components adhering to Miqu Ecosystem standards:

```
┌────────────────────────────────────────────────────────┐
│             miqudm Daemon (System Service)             │
│  • Runs as root (systemd miqudm.service)               │
│  • Manages VT / seat / logind                          │
│  • Discovers desktop sessions & system users           │
│  • Launches unprivileged greeter session               │
│  • Handles asynchronous PAM authentication             │
│  • Drops privileges & execs chosen user session        │
└──────────────────────────┬─────────────────────────────┘
                           │ IPC Socket (/run/miqudm/socket)
                           ▼
┌────────────────────────────────────────────────────────┐
│             miqudm-greeter (Pure miqutoolkit UI)       │
│  • Completely decoupled Wayland client                 │
│  • Runs inside ANY kiosk compositor (cage, miquland...) │
│  • Zero raw Cairo bypass — 100% miqutoolkit views      │
│  • User list with avatars, PAM prompt, session picker  │
│  • System power controls (Suspend, Reboot, Poweroff)   │
│  • Two-Tier Configuration + Dynamic miqutheme reload   │
│  • '--test' mode: Test live in your current desktop!   │
└────────────────────────────────────────────────────────┘
```

---

## Features

- **Pure Wayland**: Zero X11/legacy overhead; built for modern Wayland compositors (`miquland`, Sway, Hyprland, etc.).
- **Compositor Decoupled**: The greeter runs on any kiosk compositor (e.g., `cage -s -- /usr/bin/miqudm-greeter`, `miquland`, or `labwc`), completely independent of `miquland`.
- **Pure `miqutoolkit` UI**: Styled strictly using `miqutoolkit` widgets (`CardView`, `AvatarView`, `TextView`, `EditText`, `Spinner`, `Button`) with zero raw Cairo bypass.
- **Two-Tier Configuration**: Follows the Miqu Ecosystem doctrine with system baseline (`/usr/share/miqudm/miqudm.conf`) and on-demand user configuration (`miqudm-greeter --init-config`) with safe timestamped archiving.
- **Session Auto-Discovery**: Automatically parses and displays available desktop sessions from `/usr/share/wayland-sessions/` and `/usr/local/share/wayland-sessions/`.
- **User Discovery**: Scans interactive system users (UID >= 1000) and displays avatars from `~/.face`, `~/.face.icon`, or AccountsService.
- **PAM Authentication**: Full Linux-PAM authentication stack with keyring support and secure memory zeroing (`explicit_bzero`).
- **Systemd Logind Integration**: System power management (Suspend, Reboot, Power Off) and VT/Seat management via `sd-bus`.
- **Local Test Mode**: Run `miqudm-greeter --test` directly inside your existing desktop session without switching VTs or needing root privileges.

---

## Build & Installation

### Requirements
- C++20 compiler (`g++` or `clang++`)
- `CMake` (>= 3.20)
- `pkg-config`
- `miqutoolkit`
- `libpam`
- `libsystemd`
- `jsoncpp`

### Build (Local)
```bash
./make.sh
```

### Install (System-wide to `/usr`)
```bash
sudo ./make.sh
```

### Enable systemd service
```bash
sudo systemctl enable miqudm.service
```

---

## Testing / Development

You can preview and test the greeter UI without changing display managers or switching VTs:
```bash
./build/miqudm-greeter --test
```
In test mode, the window opens right inside your active desktop environment with mock users and sessions. Press `Escape` or close the window to exit.

---

## Configuration (`/etc/miqudm/miqudm.conf` / `~/.config/miqudm/miqudm.conf`)

To initialize your user configuration with dynamic `miqutheme` support:
```bash
miqudm-greeter --init-config
```
