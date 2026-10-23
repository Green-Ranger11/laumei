// Modules with no (or a partial) macOS counterpart must load and stay
// inert: no notifications, no tray, PAM never succeeds.
import QtQuick
import Quickshell
import Quickshell.Wayland
import Quickshell.Hyprland
import Quickshell.Services.Notifications
import Quickshell.Services.SystemTray
import Quickshell.Services.Pam
import Quickshell.Services.UPower
import "../lib"

ShellRoot {
    id: root
    property Check t: Check {}
    property bool pamError: false
    property bool pamCompleted: false

    property NotificationServer server: NotificationServer { actionsSupported: true }
    property PamContext pam: PamContext {
        config: "login"
        onError: root.pamError = true
        onCompleted: root.pamCompleted = true
    }
    property WlSessionLock lock: WlSessionLock { WlSessionLockSurface {} }
    property IdleInhibitor inhibitor: IdleInhibitor {}
    property IdleMonitor idle: IdleMonitor { timeout: 3600 }
    property GlobalShortcut shortcut: GlobalShortcut { name: "test" }
    property HyprlandFocusGrab grab: HyprlandFocusGrab {}
    property QsMenuAnchor menu: QsMenuAnchor {}
    property ScreencopyView screencopy: ScreencopyView {}

    Component.onCompleted: {
        t.check("NotificationServer empty", server.trackedNotifications.values.length === 0);
        t.check("SystemTray empty", SystemTray.items.values.length === 0);
        t.check("ToplevelManager empty", ToplevelManager.toplevels.values.length === 0);
        t.check("WlSessionLock unlocked", lock.locked === false);
        inhibitor.enabled = true;
        t.check("IdleInhibitor on", inhibitor.enabled === true);
        inhibitor.enabled = false;
        t.check("IdleInhibitor off", inhibitor.enabled === false);
        t.check("IdleMonitor not idle", idle.isIdle === false);
        t.check("PowerProfiles readable", [PowerProfile.PowerSaver, PowerProfile.Balanced].includes(PowerProfiles.profile));
        t.check("ScreencopyView empty", screencopy.hasContent === false);
        pam.start();
        later.start();
    }

    property Timer later: Timer {
        interval: 500
        onTriggered: {
            t.check("PamContext errors", root.pamError === true);
            t.check("PamContext never completes", root.pamCompleted === false);
            t.finish();
        }
    }
}
