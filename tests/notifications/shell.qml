// NotificationServer fed by the compat notify-send: the tracked / dismissed
// lifecycle, replacing (-r) and actions with a waiting sender (-A).
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
            // Keep the first one and the one with actions.
            if (root.received.length === 1 || n.summary === "Ask") n.tracked = true;
            if (root.received.length === 1) n.closed.connect(reason => root.closedReason = reason);
            if (n.summary === "Ask") Qt.callLater(() => n.actions.find(a => a.identifier === "yes").invoke());
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
        onExited: root.replace.running = true
    }

    property Process replace: Process {
        command: ["notify-send", "-r", "1", "Hello again"]
        onExited: root.ask.running = true
    }

    property Process ask: Process {
        command: ["notify-send", "-A", "yes=Yes", "-A", "No", "Ask"]
        stdout: StdioCollector { id: answer }
        onExited: root.check()
    }

    function check() {
        t.check("replace sends no new notification", received.length === 3, received.length);
        const n = received[0];
        t.check("replaced in place", n.summary === "Hello again", n.summary);
        t.check("body", n.body === "", n.body);
        t.check("appName", n.appName === "", n.appName);
        t.check("actions", received[2].actions.map(a => a.identifier).join() === "yes,1");
        t.check("invoked action printed", answer.text.trim() === "yes", JSON.stringify(answer.text));
        t.check("only tracked are kept", server.trackedNotifications.values.length === 2);
        n.dismiss();
        t.check("dismissed reason", closedReason === NotificationCloseReason.Dismissed, closedReason);
        t.check("untracked on dismiss", server.trackedNotifications.values.length === 1);
        t.finish();
    }

    Component.onCompleted: send.running = true
}
