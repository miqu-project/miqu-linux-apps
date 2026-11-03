# miqudm - Wayland Display Manager

`miqudm` is a lightweight, pure Wayland Display Manager and Session Greeter designed for the modern Wayland desktop ecosystem.

---

## 🏛️ Architecture

`miqudm` is strictly decomposed into two decoupled components:

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
│             miqudm-greeter (Wayland UI Client)         │
│  • Completely decoupled Wayland client                 │
│  • Runs inside a minimal kiosk compositor (cage)       │
│  • Styled with miqutoolkit views (Cairo + Pango)       │
│  • User list with avatars, PAM prompt, session picker  │
│  • System power controls (Suspend, Reboot, Poweroff)   │
│  • Single-file authoritative config (/etc/miqudm/)     │
│  • '--test' mode: Test live in your current desktop!   │
└────────────────────────────────────────────────────────┘
```

---

## 🛠️ Developer Notes: The Journey to a Pure Wayland Display Manager

Building a Display Manager in modern Wayland is vastly different from legacy X11. In X11, a shared `Xorg` server managed the screen. In Wayland, **there is no screen without a compositor**. Every pixel, seat event, and frame cycle must be driven by an active Wayland compositor.

Here is how we architected and achieved this cleanly across the stack:

### 1. The Compositor Evolution: From `miquland` to `cage`
- **Initial Exploration (`miquland -s`)**: We initially tested spawning a single-session instance of our native compositor `miquland`. While functional, running a full desktop tiling/floating window manager for a single login dialog introduced unnecessary compositor complexity and state tracking.
- **The Kiosk Breakthrough (`cage`)**: We pivoted to `cage`—a lightweight, battle-tested wlroots kiosk compositor purpose-built to display exactly one application fullscreen. Cage handles KMS/DRM modesetting, seat abstraction, and buffer swapping with near-instant boot times and zero window-management bloat.

### 2. High-Fidelity UI with `miqutoolkit`
- Display managers traditionally suffered from crude, static toolkits or heavyweight web runtimes.
- `miqudm-greeter` was built on **`miqutoolkit`**, leveraging native Wayland surface abstraction, Cairo/Pango typography, and modern widgets (`CardView`, `AvatarView`, `Spinner`, `Button`, `EditText`).
- This gives `miqudm` smooth fractional scaling, crisp anti-aliased fonts, and a native Material Design 3 light theme directly on the raw display server without external runtime bloat.

### 3. Conquering the Wayland DM Hurdles
- **The Touchpad Tap-to-Click Puzzle**: We discovered that `cage` lacks input configuration flags, and `libinput` strictly disables tap-to-click on physical clickpads by default. To solve this without patching or forking upstream Cage, we engineered `libmiqudm-kiosk-input.so`—a microscopic LD_PRELOAD hook that intercepts `wlr_cursor_attach_input_device`, instantly enables tap-to-click and natural scroll, and immediately unsets itself so child sessions remain 100% untouched.
- **Eliminating the DRM Handoff Flicker**: When Cage closed to hand DRM master over to `miquland`, the kernel's `fbcon` briefly flashed initial Arch Linux boot logs. We resolved this by issuing VT terminal blanking (`\033[H\033[2J\033[3J\033[?25l`) and configuring `TTYVTDisallocate=yes` in the systemd service, achieving a seamless, black crossfade directly into the desktop session.
- **Deterministic Pre-Session Authority**: Because `miqudm` runs as a system daemon before any user logs in, it strictly avoids touching unauthenticated user directories (`~/.config/`). `/etc/miqudm/miqudm.conf` serves as the single authoritative configuration for daemon behavior, wallpaper, and theme colors.

---

## ✨ Features

- **Pure Wayland**: Zero X11/legacy overhead; built for modern Wayland compositors (`miquland`, Sway, Hyprland, River, etc.).
- **Compositor Decoupled**: The greeter runs on any kiosk compositor (default: `cage -s -- /usr/bin/miqudm-greeter`, with `miquland` fallback).
- **Smooth Input**: Touchpad tap-to-click, tap-to-drag, and touchscreens configured automatically.
- **Zero DRM/TTY Flicker**: Clean hardware VT memory disallocation and terminal resetting.
- **Session Auto-Discovery**: Automatically parses and displays available desktop sessions from `/usr/share/wayland-sessions/` and `/usr/local/share/wayland-sessions/`.
- **User Discovery**: Scans interactive system users (UID >= 1000) and displays avatars from `~/.face`, `~/.face.icon`, or AccountsService.
- **PAM Authentication**: Full Linux-PAM authentication stack with keyring support and secure memory zeroing (`explicit_bzero`).
- **Systemd Logind Integration**: System power management (Suspend, Reboot, Power Off) and VT/Seat management via `sd-bus`.
- **Offline Test Mode**: Run `miqudm-greeter --test` directly inside your existing desktop session without switching VTs or needing root privileges.

---

## ⚙️ Configuration (`/etc/miqudm/miqudm.conf`)

Configuration is managed authoritatively at `/etc/miqudm/miqudm.conf` (with system baseline at `/usr/share/miqudm/miqudm.conf`).

```ini
[General]
default_session = miquland
remember_last_user = true
last_user = anisur
greeter_command = cage -s -- /usr/bin/miqudm-greeter
greeter_user = greeter
vt = 1
numlock = false

[Greeter]
wallpaper =
clock_format_24h = true
show_power_buttons = true
show_sessions = true
allow_user_switching = true
card_radius = 20
avatar_size = 84

[Theme]
background = #f6f0fa
on_background = #1d192b
surface = #ffffff
on_surface = #1d192b
surface_variant = #f0eaf7
on_surface_variant = #49454f
primary = #6750a4
on_primary = #ffffff
primary_container = #eaddff
on_primary_container = #21005d
outline = #79747e
outline_variant = #d0c8da
backdrop = #00000033
```

### 🧩 Toolkit Resolution & Override Mechanism

Understanding how configuration flows through `miqutoolkit` and `miqudm`:

1. **Where `miqutoolkit` gets its widget baseline**:
   - Because `miqudm-greeter` runs before login as the unprivileged system user `greeter` (`$HOME=/var/empty`), it cannot access any personal user directories (`~/.config/miqutoolkit/`).
   - `miqutoolkit` therefore initializes its system baseline from:
     ```
     /usr/share/miqutoolkit/miqutoolkit.conf
     ```
   - From here, it establishes default typography (`font = Sans`, `font_size = 11`, `h1_size = 20`), default metrics (`border_width = 1`, `card_elevation = 1.0`), and generic fallback colors.

2. **When and what gets overridden**:
   - Immediately after toolkit initialization, `miqudm-greeter` loads `/etc/miqudm/miqudm.conf`.
   - **Colors**: All tokens in the `[Theme]` block explicitly **overwrite** `miqutoolkit`'s baseline colors with `miqudm`'s dedicated palette.
   - **Metrics & Geometry**: `card_radius`, `avatar_size`, and `wallpaper` in `[Greeter]` directly configure the login card and background.
   - **Fallback Safety**: Any design token *not* explicitly redefined in `miqudm.conf` safely retains the consistent system baseline from `miqutoolkit`.


---

## 📦 Build & Installation

### Requirements
- C++20 compiler (`g++` or `clang++`)
- `CMake` (>= 3.20)
- `pkg-config`
- `miqutoolkit`
- `cage` (or other kiosk Wayland compositor)
- `libpam`
- `libsystemd`
- `jsoncpp`
- `wlroots-0.20` (for kiosk input hook)
- `libinput`

### Build (Local)
```bash
./make.sh
```

### Install (System-wide to `/usr` & `/etc`)
```bash
sudo ./make.sh
```

### Enable systemd service
```bash
sudo systemctl enable miqudm.service
```

---

## 🧪 Testing / Development

You can preview the greeter UI offline without stopping your active session:
```bash
./build/miqudm-greeter --test
```
You can also preview with a custom configuration file:
```bash
./build/miqudm-greeter --test -c assets/miqudm.conf
```
Press `Escape` or close the window to exit.
