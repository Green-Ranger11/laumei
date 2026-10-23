import QtQuick
import QtQuick.Window

// Quickshell's PopupWindow: a borderless, focus-less window placed against
// `anchor` (an item in another window, usually a bar). Kept on screen.
Window {
    id: popup

    readonly property PopupAnchor anchor: PopupAnchor {}
    property real implicitWidth: 0
    property real implicitHeight: 0
    // Older Quickshell API: position relative to parentWindow.
    property var parentWindow: null
    property real relativeX: 0
    property real relativeY: 0

    visible: false
    flags: Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint | Qt.WindowDoesNotAcceptFocus
    width: implicitWidth > 0 ? implicitWidth : 100
    height: implicitHeight > 0 ? implicitHeight : 100

    function _place() {
        const a = anchor;
        let rx, ry, rw, rh;
        if (a.item && a.item.Window.window) {
            const r = a.rect;
            const p = a.item.mapToGlobal(r.x, r.y);
            rx = p.x;
            ry = p.y;
            rw = r.width > 0 ? r.width : a.item.width;
            rh = r.height > 0 ? r.height : a.item.height;
        } else {
            const w = a.window ?? parentWindow;
            if (!w) return;
            const r = a.window ? a.rect : Qt.rect(relativeX, relativeY, 0, 0);
            rx = w.x + r.x;
            ry = w.y + r.y;
            rw = r.width;
            rh = r.height;
        }

        const e = a.edges, g = a.gravity, m = a.margins;
        const px = (e & Edges.Left) ? rx : (e & Edges.Right) ? rx + rw : rx + rw / 2;
        const py = (e & Edges.Top) ? ry : (e & Edges.Bottom) ? ry + rh : ry + rh / 2;
        let x = (g & Edges.Left) ? px - width : (g & Edges.Right) ? px : px - width / 2;
        let y = (g & Edges.Top) ? py - height : (g & Edges.Bottom) ? py : py - height / 2;
        x += m.left - m.right;
        y += m.top - m.bottom;

        // Keep it on the screen it opened on.
        const s = Qt.application.screens.find(s => px >= s.virtualX && px < s.virtualX + s.width
                                                  && py >= s.virtualY && py < s.virtualY + s.height);
        if (s) {
            x = Math.max(s.virtualX, Math.min(x, s.virtualX + s.width - width));
            y = Math.max(s.virtualY, Math.min(y, s.virtualY + s.height - height));
        }
        popup.x = Math.round(x);
        popup.y = Math.round(y);
    }

    onVisibleChanged: {
        if (!visible) return;
        _place();
        // Above bars (see PanelWindow for why this waits a tick).
        Qt.callLater(() => {
            if (!popup.visible) return;
            QsMacWindow.configure(popup, 3, 0);
            popup._place();
        });
    }
    onWidthChanged: if (visible) _place()
    onHeightChanged: if (visible) _place()
}
