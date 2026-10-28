// Quickshell.Hyprland on the AeroSpace backend, against tests/aerospace/bin/aerospace.
import QtQuick
import Quickshell
import Quickshell.Hyprland
import Quickshell.Io
import Quickshell.Wayland
import "../lib"

ShellRoot {
    property Check t: Check {}
    // Singletons are created on first use; start the backend now.
    Component.onCompleted: Hyprland.workspaces
    property FileView log: FileView { path: Quickshell.env("AEROSPACE_MOCK_LOG") ?? ""; printErrors: false }

    property Process hyprctl: Process { command: ["hyprctl", "dispatch", "movefocus", "r"] }

    property Timer settle: Timer {
        interval: 1500; running: true
        onTriggered: {
            const ids = Hyprland.workspaces.values.map(w => w.id);
            // Workspace 3 has no windows and isn't visible, so it doesn't exist.
            t.check("workspaces", JSON.stringify(ids) === "[1,2,1000]", JSON.stringify(ids));
            t.check("non-numeric name kept", Hyprland.workspaces.values[2]?.name === "web");
            t.check("focused workspace", Hyprland.focusedWorkspace?.id === 1);
            t.check("toplevels per workspace", Hyprland.focusedWorkspace?.toplevels.values.length === 1);
            t.check("monitor name", Hyprland.focusedMonitor?.name === "Test Display", Hyprland.focusedMonitor?.name);
            t.check("ToplevelManager toplevels", ToplevelManager.toplevels.values.length === 3);
            t.check("activeToplevel", ToplevelManager.activeToplevel?.appId === "kitty", ToplevelManager.activeToplevel?.appId);
            ToplevelManager.toplevels.values.find(w => w.appId === "Firefox").activate();
            Hyprland.dispatch("killactive");
            Hyprland.dispatch("movefocus l");
            Hyprland.dispatch("togglefloating");
            hyprctl.running = true;
            Hyprland.dispatch("hl.dsp.focus({ workspace = 2 })");
            Hyprland.dispatch("movetoworkspace web");
            dispatched.start();
        }
    }

    property Timer dispatched: Timer {
        interval: 800
        onTriggered: {
            log.reload();
            const lines = log.text().trim().split("\n");
            t.check("dispatch focus", lines.includes("workspace 2"), JSON.stringify(lines));
            t.check("dispatch move", lines.includes("move-node-to-workspace --focus-follows-window web"));
            t.check("Toplevel.activate", lines.includes("focus --window-id 12"), JSON.stringify(lines));
            t.check("killactive", lines.includes("close"));
            t.check("movefocus", lines.includes("focus left"));
            t.check("togglefloating", lines.includes("layout floating tiling"));
            t.check("hyprctl dispatch", lines.includes("focus right"), JSON.stringify(lines));
            t.finish();
        }
    }
}
