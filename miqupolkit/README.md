# MiquPolkit 🛡️

A lightweight, native Wayland graphical Polkit authentication agent built with **`miqutoolkit`** for the Miqu desktop environment.

MiquPolkit provides secure privilege escalation dialogs for administrative tasks, adhering strictly to Wayland layer-shell protocols and the Miqu design doctrine.

---

## ✨ Features

- 🔐 **Polkit D-Bus Integration**: Implements the `org.freedesktop.PolicyKit1.AuthenticationAgent` interface.
- 🎨 **Miqu Design Tokens**: Uses `miqutoolkit` typography, corner radii, and color palette tokens.
- 🪟 **Wayland Layer Shell**: Centered modal overlay on the active Wayland compositor with keyboard grabbing.
- ⚡ **Lightweight & Fast**: Direct C++20 implementation with zero heavy desktop dependencies.

---

## 🛠️ Build & Installation

### Prerequisites
- C++20 compiler (`gcc` or `clang`)
- CMake 3.20+
- `miqutoolkit`
- `polkit-gobject-1`

### Local Build
```bash
./make.sh
```

### System Installation
To build and install system-wide to `/usr`:
```bash
sudo ./make.sh
```

---

## ⚙️ Configuration

MiquPolkit follows the uniform Miqu two-tier configuration architecture:
- Baseline configuration: `/usr/share/miqupolkit/miqupolkit.conf`
- Generate user configuration:
```bash
miqupolkit --init-config
```
This safely creates or archives your user configuration at `~/.config/miqupolkit/miqupolkit.conf`.
