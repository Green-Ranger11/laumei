# Laumei

Run [Quickshell](https://quickshell.org) configs on macOS.

Quickshell is Linux-only (Wayland/X11). This is a separate runtime that
provides Quickshell's QML modules on top of plain Qt, so QML written for
Quickshell loads unchanged where macOS can host it. It installs a `laumei`
command with the same CLI as Quickshell's `qs`.

## What Laumei does and doesn't do

**It does:** load Quickshell configs on macOS and run everything macOS can
host: panels and bars, pickers and launchers, popups, workspaces (from yabai
or AeroSpace), audio, battery, brightness, media, Wi-Fi, Bluetooth, clipboard
history, system stats, IPC and hot reload. Linux commands configs commonly
run (`wl-copy`, `notify-send`, `brightnessctl`, `systemctl suspend`, ...) are
translated.

**It loads but can't do** (the types exist, so configs load; they stay inert):

| Feature | Why not on macOS |
|---|---|
| Notification server (`NotificationServer`) | Apps post to Notification Center; macOS gives no way for another app to receive them. |
| System tray (`SystemTray`, `QsMenuAnchor`) | macOS 27 draws every menu-bar item into one shared window, so there are no per-item icons or menus to mirror. |
| Custom lock screens (`WlSessionLock`, `PamContext`) | Only macOS's own lock screen is trusted (Touch ID, Apple Watch, FileVault). Locking hands off to it; the lock UI is never shown, and `PamContext` never authenticates. |
| Exclusive zones | A window can't reserve screen space; set it in the window manager (yabai `external_bar`, AeroSpace `outer.top`). |
| Global shortcuts (`GlobalShortcut`) | Bind keys in skhd (or similar) to `laumei ipc call ...`. |
| Focus grabs, screencopy, other apps' windows (`HyprlandFocusGrab`, `ScreencopyView`, `ToplevelManager`) | Wayland/Hyprland protocols with no counterpart Laumei can use. |
| Hyprland beyond workspaces | Laumei maps workspaces and monitors onto yabai/AeroSpace; other dispatchers are logged and ignored. |
| Switching power profiles | Low Power Mode is readable but needs admin rights to change. |

**Partly:**

- **Bluetooth:** no trusted/blocked flags on macOS (paired means trusted).
  Battery levels only for devices that report them to macOS (AirPods, Beats,
  some headphones).
- **Wi-Fi:** network names need Location access. Forgetting a network asks
  for an admin password. `autoconnect` has no effect.
- **Media:** macOS has one system-wide "Now Playing" session, so there is at
  most one player. No artwork yet.

## Status

| Module | Types | Notes |
|---|---|---|
| `Quickshell` | `ShellRoot`, `Scope`, `Singleton`, `Variants`, `LazyLoader`, `PanelWindow`, `FloatingWindow`, `PopupWindow`, `Edges`, `SystemClock`, `Region`, `ExclusionMode`, `DesktopEntries`, `QsMenuAnchor`, `QsMenuHandle`, `Quickshell` singleton (`screens`, `env`, `iconPath`, `execDetached`, `shellDir`, `stateDir`, `reload`, ...) | `DesktopEntries` lists `.app` bundles; `iconPath` resolves app bundles, files and app names. `Region` masks are accepted but ignored. |
| `Quickshell.Io` | `Process`, `StdioCollector`, `SplitParser`, `FileView`, `IpcHandler` | `FileView` synthesizes `/proc/stat`, `/proc/meminfo`, `/sys/class/thermal/thermal_zone0/temp` (CPU die sensors), `/sys/class/backlight/*/{brightness,max_brightness}` (built-in display) and `/etc/hostname`. |
| `Quickshell.Wayland` | `WlrLayershell` (attached), `WlrLayer`, `WlrKeyboardFocus`, `WlSessionLock`, `WlSessionLockSurface`, `IdleInhibitor`, `IdleMonitor`, `ScreencopyView`, `ToplevelManager` | Layers map to NSWindow levels. `WlSessionLock` locks with the macOS lock screen. `IdleInhibitor` holds a display-sleep assertion; `IdleMonitor` reads the system idle time. |
| `Quickshell.Widgets` | `IconImage` | |
| `Quickshell.Hyprland` | `Hyprland` (`workspaces`, `monitors`, `toplevels`, `focusedWorkspace`, `focusedMonitor`, `dispatch`), `HyprlandFocusGrab`, `GlobalShortcut` | yabai or AeroSpace (below). Monitor names match `Quickshell.screens`. |
| `Quickshell.Services.Pipewire` | `Pipewire`, `PwNode`, `PwNodeAudio`, `PwObjectTracker` | CoreAudio devices as nodes; default output/input as sink/source. |
| `Quickshell.Services.UPower` | `UPower`, `UPowerDevice`, `UPowerDeviceState`, `UPowerDeviceType`, `PowerProfiles`, `PowerProfile` | IOKit power sources. `PowerProfiles` reports Low Power Mode as `PowerSaver`. |
| `Quickshell.Services.Mpris` | `Mpris`, `MprisPlayer`, `MprisPlaybackState` | Via [`media-control`](https://github.com/ungive/media-control) (`brew install media-control`); Apple restricts the MediaRemote framework. |
| `Quickshell.Bluetooth` | `Bluetooth`, `BluetoothAdapter`, `BluetoothDevice`, `BluetoothDeviceState`, `BluetoothAdapterState` | IOBluetooth, with connection state from `system_profiler` (IOBluetooth misreports it on recent macOS). |
| `Quickshell.Networking` | `Networking`, `WifiDevice`, `WifiNetwork`, `DeviceType`, `ConnectionState`, `WifiSecurityType`, `ConnectionFailReason` | CoreWLAN + `networksetup`. |
| `Quickshell.Services.Notifications` | `NotificationServer`, `Notification`, `NotificationAction`, `NotificationUrgency`, `NotificationCloseReason` | Inert (see above). |
| `Quickshell.Services.SystemTray` | `SystemTray`, `SystemTrayItem`, `Status`, `Category` | Always empty (see above). |
| `Quickshell.Services.Pam` | `PamContext`, `PamResult`, `PamError` | `start()` always ends in `error()`. |

### Workspaces: yabai or AeroSpace

`Quickshell.Hyprland` uses yabai when it's installed, otherwise AeroSpace; set
`LAUMEI_WM=yabai` or `LAUMEI_WM=aerospace` to choose. yabai reports changes
through signals Laumei registers on start; AeroSpace is polled once a second.
A yabai space labelled `ws<N>` (`yabai -m space --label ws2`) is workspace N
wherever it sits, so a workspace can live on another display; unlabelled spaces
use their yabai index.
AeroSpace workspaces named with a number keep it as their id; others are
numbered from 1000 and keep their name.

### How it maps to macOS

- **PanelWindow** is a borderless window placed from `anchors`, `margins` and
  `implicitWidth`/`implicitHeight`. `WlrLayershell.layer` picks the window
  level (`Overlay` sits above the menu bar). With a keyboard focus mode other
  than `None` it takes focus when shown and hands it back to the previous app
  when hidden. Unpinned windows open on the screen under the mouse, standing in
  for the focused output.
- **The native menu bar.** Top-edge panels at the `Top` layer or above step
  aside while an auto-hidden menu bar is slid down, so it stays readable.
- **No exclusive zones.** macOS has no way for a window to reserve screen
  space; reserve it in your window manager (e.g. yabai's `external_bar`).
- **Linux tools.** `Process` puts the bundle's `compat/bin` first on `PATH`
  (then your `PATH`, then `~/.local/bin` and Homebrew):
  - `wl-copy` / `wl-paste`, including `--sensitive` (marked concealed, so
    clipboard managers skip it), `--clear`, `--type` and `--watch` (so
    `wl-paste --watch cliphist store` keeps a history; build `cliphist` with
    `go install go.senan.xyz/cliphist@latest`)
  - `notify-send` -> macOS notifications
  - `systemctl suspend|hibernate|reboot|poweroff|is-active` -> `pmset` / System Events
  - `loginctl lock-session|kill-session` -> lock screen / log out
  - `systemd-inhibit` -> `caffeinate`
  - `brightnessctl get|set|info` -> built-in display brightness
- **Permissions.** Laumei is an app bundle (`Laumei.app`, no Dock icon) so macOS
  can ask for Location (Wi-Fi names) and Bluetooth access. Builds are ad-hoc
  signed, so a rebuild may ask again.
- **Hot reload.** Every QML/qmldir/JS file the shell loads is watched,
  including ones imported from other directories. A failed reload keeps the
  running config.

## Build

Needs Qt 6.5+ and CMake.

```sh
brew install qt cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
cmake --build build
cmake --install build --prefix ~/.local   # ~/.local/Applications/Laumei.app, ~/.local/bin/laumei
```

## Test

```sh
tests/run.sh            # every tests/<name>/shell.qml against build/Laumei.app
```

Each test is a small config that checks behaviour and exits non-zero on
failure (`tests/lib/Check.qml`). `tests/aerospace` runs against a fake
`aerospace` CLI in `tests/aerospace/bin`.

## Use

```sh
laumei -c mac                           # ~/.config/quickshell/mac/shell.qml
laumei -p path/to/shell.qml             # or a directory holding one
laumei -c mac ipc show                  # list IpcHandler targets
laumei -c mac ipc call launcher toggle
laumei -c mac kill
```

Run it at login with the LaunchAgent in `contrib/` (instructions inside).

If you use yabai, tell it to leave Laumei's windows alone:

```sh
yabai -m rule --add app="^Laumei$" manage=off
```

### Sharing a config with Linux

A config written for Linux loads as it is: what macOS can't do stays inert.
To leave out parts that only make sense on Linux (or lay a bar out around the
notch), give macOS its own `shell.qml` that imports the shared pieces:

```qml
// ~/.config/quickshell/mac/shell.qml
import Quickshell
import "../mocha/launcher"

ShellRoot {
    AppLauncher {}
    EmojiPicker {}
}
```

Widgets that call the shell's own CLI (`qs ipc call ...` on Linux) should pick
the command by platform:

```qml
readonly property string shellCommand: Qt.platform.os === "osx" ? "laumei" : "qs"
```

