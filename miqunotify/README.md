# miqunotify

Lightweight, modern Wayland desktop notification daemon and slide-out notification center for the Miqu ecosystem built natively with `miqutoolkit` and `sd-bus`.

## Features
- **FreeDesktop Notifications v1.2**: Full `org.freedesktop.Notifications` compatibility.
- **Corner Toast Banners**: Floating toasts with application icons, timestamps, action buttons, timer progress bars, and hover-to-pause.
- **Slide-out Notification Center**: Persistent drawer (`miqunotify --toggle`) tracking notification history, DND toggle, and clear-all actions.
- **Do Not Disturb (DND)**: Suppresses popup banners while queuing notifications in history.
- **Runtime IPC & Miqubar Badge**: Updates `/run/user/<uid>/miqunotify_state` so `miqubar` can display live unread badge counters (`[ 🔔 3 ]`).
- **Unified Theming**: Seamlessly inherits typography, corner radiuses, and color palettes from `miqutoolkit`.

## Usage
Start daemon:
```bash
miqunotify
```

Toggle Notification Center drawer:
```bash
miqunotify --toggle
```

Generate default user configuration:
```bash
miqunotify --init-config
```

Send a test notification:
```bash
notify-send "Welcome to Miqu" "This is a native notification banner."
```
