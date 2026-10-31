# miqufiles

<p align="center">
  <b>Modern Yazi-style Miller Columns File Manager for the Miqu Desktop Ecosystem.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-blue?style=flat-square" alt="C++20" />
  <img src="https://img.shields.io/badge/Toolkit-miqutoolkit-purple?style=flat-square" alt="miqutoolkit" />
  <img src="https://img.shields.io/badge/Display-Wayland%20Native-orange?style=flat-square" alt="Wayland Native" />
  <img src="https://img.shields.io/badge/License-GPL--3.0-green?style=flat-square" alt="GPLv3" />
</p>

---

## 🌟 Overview

`miqufiles` (Display name: **Files**) is a lightweight, responsive Wayland-native file manager featuring a **Yazi-style 3-column Miller Columns GUI**:
1. **Left Column (Parent Directory)**: Shows parent directory context with the current folder highlighted.
2. **Center Column (Active Directory)**: Fast keyboard/mouse-driven browsing of current directory items.
3. **Right Column (Contextual Preview)**: Peeks into child directory contents if a folder is selected, or renders rich file previews (images, text/code with syntax styling, or binary metadata) when a file is selected.

---

## ⌨️ Controls & Shortcuts

| Action | Shortcuts |
|---|---|
| **Navigate Up (Parent)** | `h`, `Left Arrow`, `Backspace`, Toolbar Back Button |
| **Enter Directory / Open File** | `l`, `Right Arrow`, `Return` / `Enter`, Double Click |
| **Move Selection Up / Down** | `k` / `j`, `Up` / `Down` Arrow, Mouse Click / Scroll |
| **Jump to Top / Bottom** | `g` / `G`, `Home` / `End` |
| **Page Up / Page Down** | `Page Up` / `Page Down` |
| **Toggle Hidden Files (Dotfiles)** | `.`, `Ctrl+H`, Toolbar Dotfiles Button |
| **Jump to Home (`$HOME`)** | `~`, Toolbar Home Button |
| **Search / Filter Files** | `/`, Toolbar Search Button |
| **Refresh Directory** | `F5`, `Ctrl+R`, Toolbar Reload Button |
| **Quit** | `q`, `Escape` (when not in search), Window Close Button |

---

## ⚙️ Configuration

`miqufiles` strictly adheres to the **Uniform Two-Tier Single-File Authority Model**:
- System baseline: `/usr/share/miqufiles/miqufiles.conf`
- User config: `~/.config/miqufiles/miqufiles.conf`

To generate a documented user configuration with safe timestamped archiving:
```bash
miqufiles --init-config
```

---

## 🔨 Build & Install

```bash
# Build locally
./make.sh

# Install system-wide
sudo ./make.sh
```
