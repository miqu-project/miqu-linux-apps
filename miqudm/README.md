# miqudm - Wayland Display Manager

> [!WARNING]
> **Status: Unusable for now**
> `miqudm` is under active development and is currently unusable / not ready for production use.

`miqudm` is a lightweight, pure Wayland Display Manager and Session Greeter designed for the `miqu` desktop ecosystem and modern Linux systems.

---

## Features

- **Pure Wayland First**: Zero X11/legacy overhead; built specifically for modern Wayland compositors (`miquland`, Hyprland, Sway, GNOME Wayland, KDE Plasma Wayland, etc.).
- **Material-You Themed UI**: Powered by `miqutoolkit` with responsive user cards, avatar/initial display, custom themes, and animations.
- **Session Auto-Discovery**: Automatically discovers and displays available Wayland desktop sessions from `/usr/share/wayland-sessions/` and `/usr/local/share/wayland-sessions/`.
- **User Discovery**: Scans interactive system users (UID >= 1000) and displays avatars from `~/.face`, `~/.face.icon`, or AccountsService.
- **PAM Authentication & Session Setup**: Complete Linux-PAM authentication stack with keyring (`pam_gnome_keyring`, `pam_kwallet5`) support and secure memory zeroing (`explicit_bzero`).
- **Systemd Logind Integration**: System power management (Suspend, Reboot, Power Off, Hibernate) and VT/Seat management via `sd-bus`.
- **Modular IPC Architecture**: Minimal daemon (`miqudm`) communicating with greeter UI (`miqudm-greeter`) over Unix Domain Sockets with JSON-RPC messaging.
- **Preview & Test Mode**: Run `miqudm-greeter --test` directly inside an existing desktop session for rapid UI customization and testing without VT switching.

---

## Architecture

```
                       +-----------------------------------+
                       |        miqudm-greeter (UI)        |
                       |       (Built with miqutoolkit)    |
                       +-----------------+-----------------+
                                         | Unix Domain Socket
                                         | (/run/miqudm/socket)
                                         v
+-------------------------------------------------------------------------+
|                              miqudm Daemon                              |
|                                                                         |
|   +-------------------+    +--------------------+    +--------------+   |
|   |   PAM Session     |    |   Session Manager  |    |  sd-bus /    |   |
|   | (Auth & Keyring)  |    | (Privilege Drop &  |    |  logind      |   |
|   |                   |    |   Session Exec)    |    | (Power / VT) |   |
|   +-------------------+    +--------------------+    +--------------+   |
+-------------------------------------------------------------------------+
                                         |
                                         v
                       +-----------------------------------+
                       |      Wayland User Session         |
                       |    (e.g., miquland / hyprland)    |
                       +-----------------------------------+
```

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

---

## Configuration (`/etc/miqudm/miqudm.conf`)

```ini
[General]
# Default Wayland session ID or name
default_session=miquland

# Remember last logged in user across restarts
remember_last_user=true

# Last logged in user (updated automatically)
last_user=

# Command to launch the greeter compositor and greeter UI
greeter_command=miquland -s /usr/bin/miqudm-greeter

# User account under which greeter runs
greeter_user=greeter

# Virtual terminal number (0 for auto allocation or current VT)
vt=1

# Enable NumLock on startup
numlock=false

[Autologin]
enabled=false
user=
session=miquland
```
