pragma Singleton
import QtQml
import Quickshell.Hyprland

// Other apps' windows, from the window manager (yabai or AeroSpace) through
// Quickshell.Hyprland; each is a HyprlandToplevel with Toplevel's API.
QtObject {
    readonly property var toplevels: Hyprland.toplevels
    readonly property var activeToplevel: Hyprland.toplevels.values.find(t => t.activated) ?? null
}
