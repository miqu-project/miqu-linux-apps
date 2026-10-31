# miqupower

> Modern, native Desktop Environment Power & Session Management Dialog built with `miqutoolkit`.

`miqupower` provides a smooth, keyboard-accessible session management interface with graceful shutdown integration for `miquland` and the Miqu ecosystem.

---

## Features

- **5 Core Desktop Power Actions**:
  - ⏻ **Shut Down** (`systemctl poweroff` / graceful `miquland` teardown)
  - ↻ **Restart** (`systemctl reboot` / graceful `miquland` teardown)
  - ⏾ **Sleep** (`systemctl suspend` with automatic screen lock)
  - 🔒 **Lock** (`miqulock` / `loginctl lock-session`)
  - 🚪 **Log Out** (graceful desktop session termination)
  - *(Optional)* **Hibernate** (`systemctl hibernate`)
- **Active Applications Safety Warning**:
  - Queries running application windows directly from `miquland` via IPC (`clients`).
  - Warns the user if active application windows are open before shutting down or restarting.
  - Displays application titles, icons, and workspace badges.
  - Allows the user to cancel (<kbd>Esc</kbd>) to save work or proceed (<kbd>Enter</kbd>).
- **Graceful Shutdown Visual Feedback**:
  - Stays mapped on the Wayland layer-shell overlay layer while `miquland` prompts and closes application windows.
  - Displays a clean teardown progress view with status updates and an indeterminate progress bar.
- **Ergonomic Keyboard Navigation**:
  - Hotkeys: <kbd>S</kbd> (Shut Down), <kbd>R</kbd> (Restart), <kbd>U</kbd> (Sleep), <kbd>L</kbd> (Lock), <kbd>E</kbd> (Log Out).
  - Arrow keys (<kbd>Left</kbd> / <kbd>Right</kbd> / <kbd>Tab</kbd>) + <kbd>Enter</kbd>.
  - Instant dismissal with <kbd>Esc</kbd> or click outside.
- **Direct CLI Execution**:
  - Run `miqupower` for the interactive menu.
  - Run `miqupower --shutdown` or `miqupower --reboot` for immediate graceful actions.
- **Dynamic Theming**:
  - Fully reactive to `miqutheme` through `miqutoolkit` tokens.

---

## Configuration

Following the Two-Tier Configuration Architecture:
- System baseline: `/usr/share/miqupower/miqupower.conf`
- User configuration: `~/.config/miqupower/miqupower.conf`

To generate a user configuration file:
```bash
miqupower --init-config
```

---

## Build & Installation

```bash
# Local test build
./make.sh

# System-wide install
sudo ./make.sh
```
