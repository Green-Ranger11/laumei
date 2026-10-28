// NotificationServer fed by the compat notify-send, and the tracked /
// dismissed lifecycle.
import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Services.Notifications
import "../lib"

ShellRoot {
    id: root
    property Check t: Check {}
    property var received: []
    property int closedReason: -1

    property NotificationServer server: NotificationServer {
        onNotification: n => {
            root.received = root.received.concat([n]);
            // Keep only the first one.
            if (root.received.length === 1) {
                n.tracked = true;
                n.closed.connect(reason => root.closedReason = reason);
            }
        }
    }

    property Process send: Process {
        command: ["notify-send", "-a", "Tests", "-u", "critical", "-p", "-h", "string:desktop-entry:tests",
                  "Hello", "Body text"]
        stdout: StdioCollector { id: printed }
        onExited: code => {
            t.check("notify-send exit", code === 0, code);
            t.check("printed id", printed.text.trim() === "1", printed.text);
            root.sendSecond.running = true;
        }
    }

    property Process sendSecond: Process {
        command: ["notify-send", "Dropped"]
        onExited: root.check()
    }

    function check() {
        t.check("received both", received.length === 2, received.length);
        const n = received[0];
        t.check("summary", n.summary === "Hello", n.summary);
        t.check("body", n.body === "Body text", n.body);
        t.check("appName", n.appName === "Tests", n.appName);
        t.check("urgency", n.urgency === NotificationUrgency.Critical, n.urgency);
        t.check("desktopEntry", n.desktopEntry === "tests", n.desktopEntry);
        t.check("only the tracked one is kept", server.trackedNotifications.values.length === 1);
        n.dismiss();
        t.check("dismissed reason", closedReason === NotificationCloseReason.Dismissed, closedReason);
        t.check("untracked on dismiss", server.trackedNotifications.values.length === 0);
        t.finish();
    }

    Component.onCompleted: send.running = true
}
