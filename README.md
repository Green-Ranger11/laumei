# quickshell-macos

Run [Quickshell](https://quickshell.org) configs on macOS.

Quickshell is Linux-only (Wayland/X11). This is a separate runtime that
provides Quickshell's QML modules on top of plain Qt, so QML written for
Quickshell loads unchanged where macOS can host it. It installs a `qs` binary
with the same CLI.

## Status

Early. The goal is the picker/launcher style of config first, then bars.

| Module | Types | Notes |
|---|---|---|
| `Quickshell` | `ShellRoot`, `Scope`, `Singleton`, `Variants`, `PanelWindow`, `FloatingWindow`, `Region`, `ExclusionMode`, `DesktopEntries`, `Quickshell` singleton (`screens`, `env`, `iconPath`, `execDetached`, `stateDir`, `reload`, ...) | `DesktopEntries` lists `.app` bundles; `iconPath` resolves app bundles, files and app names. `Region` masks are accepted but ignored. |
| `Quickshell.Io` | `Process`, `StdioCollector`, `SplitParser`, `FileView`, `IpcHandler` | |
| `Quickshell.Wayland` | `WlrLayershell` (attached), `WlrLayer`, `WlrKeyboardFocus` | Mapped onto NSWindow levels and app activation. |
| `Quickshell.Widgets` | `IconImage` | |
| `Quickshell.Bluetooth` | `Bluetooth`, `BluetoothAdapter`, `BluetoothDevice`, `BluetoothDeviceState`, `BluetoothAdapterState` | IOBluetooth. Power, discoverable, scan, pair ("just works"), connect, forget, battery for Apple-protocol devices. macOS has no trusted/blocked flags: paired means trusted, nothing is blocked. |
| `Quickshell.Networking` | `Networking`, `WifiDevice`, `WifiNetwork`, `DeviceType`, `WifiSecurityType`, `ConnectionFailReason` | CoreWLAN + `networksetup`. Needs Location access (macOS treats network names as location). Forgetting a network asks for an admin password. `autoconnect` is accepted but has no effect. |

Not yet: `Quickshell.Hyprland`, `Services.*` (Pipewire, UPower, Mpris,
Notifications, SystemTray, Pam), `WlSessionLock`, `PopupWindow`,
`LazyLoader`, `SystemClock`.

### How it maps to macOS

- **PanelWindow** is a borderless window placed from `anchors`, `margins` and
  `implicitWidth`/`implicitHeight`. `WlrLayershell.layer` picks the window
  level (`Overlay` sits above the menu bar). With a keyboard focus mode other
  than `None` it takes focus when shown and hands it back to the previous app
  when hidden. Unpinned windows open on the screen under the mouse, standing in
  for the focused output.
- **No exclusive zones.** macOS has no way for a window to reserve screen
  space; reserve it in your window manager (e.g. yabai's `external_bar`).
- **Linux tools.** `Process` puts the bundle's `compat/bin` first on `PATH`:
  - `wl-copy` / `wl-paste`, including `--sensitive` (marked concealed, so
    clipboard managers skip it), `--clear`, `--type` and `--watch` (so
    `wl-paste --watch cliphist store` keeps a history; build `cliphist` with
    `go install go.senan.xyz/cliphist@latest`)
  - `notify-send` -> macOS notifications
  - `systemctl suspend|hibernate|reboot|poweroff|is-active` -> `pmset` / System Events
  - `loginctl lock-session|kill-session` -> lock screen / log out
- **Permissions.** `qs` is an app bundle (`qs.app`, no Dock icon) so macOS
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
cmake --install build --prefix ~/.local   # ~/.local/Applications/qs.app, ~/.local/bin/qs
```

## Use

```sh
qs -c mac                               # ~/.config/quickshell/mac/shell.qml
qs -p path/to/shell.qml                 # or a directory holding one
qs -c mac ipc show                      # list IpcHandler targets
qs -c mac ipc call launcher toggle
qs -c mac kill
```

Run it at login with the LaunchAgent in `contrib/` (instructions inside).

If you use yabai, tell it to leave `qs` windows alone:

```sh
yabai -m rule --add app="^qs$" manage=off
```

### Sharing a config with Linux

Keep the shared pieces where your Linux config has them and give macOS its own
`shell.qml` that imports only what it can run:

```qml
// ~/.config/quickshell/mac/shell.qml
import Quickshell
import "../mocha/launcher"

ShellRoot {
    AppLauncher {}
    EmojiPicker {}
}
```
