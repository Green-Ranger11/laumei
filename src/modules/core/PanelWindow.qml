import QtQuick
import QtQuick.Window
import Quickshell.Wayland

// Quickshell's PanelWindow on macOS: a borderless window placed from
// `anchors` / `margins` / `implicitWidth|Height` on its screen, at the window
// level its WlrLayershell.layer maps to. There are no exclusive zones on
// macOS (reserve bar space in the window manager instead, e.g. yabai's
// external_bar).
Window {
    id: win

    readonly property PanelAnchors anchors: PanelAnchors {}
    readonly property PanelMargins margins: PanelMargins {}
    property real implicitWidth: 0
    property real implicitHeight: 0
    property int exclusiveZone: 0
    property int exclusionMode: ExclusionMode.Normal
    property bool focusable: false
    property bool aboveWindows: true
    property var mask: null
    property var surfaceFormat: null

    // Top-level even when declared inside an Item: Qt would otherwise make it
    // transient for that Item's window and keep it hidden while there is none.
    transientParent: null
    visible: true
    // No-focus from the first show: set later (in _configure), Qt has already
    // made the window key and pulled focus from the frontmost app.
    flags: Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint | (_wantsFocus() ? 0 : Qt.WindowDoesNotAcceptFocus)

    // A `screen` set by the config (bars bound to Variants' modelData) pins
    // the window; otherwise it opens on the screen under the mouse, standing
    // in for the compositor's focused output.
    property bool _completed: false
    property bool _screenPinned: false
    property bool _tookFocus: false
    // After completion a screen change means Qt moved the window to another
    // display, which can rebuild the NSWindow without its level and spaces
    // settings: apply them again.
    onScreenChanged: if (!_completed) _screenPinned = true; else Qt.callLater(_shown)

    function _targetScreen() {
        // A Variants delegate over Quickshell.screens (one bar per display):
        // its modelData is the screen. Qt may already have moved `screen` to
        // the display under the mouse, and binding it to the screen it
        // started on sends no change.
        if ("modelData" in win && Qt.application.screens.indexOf(win.modelData) >= 0) return win.modelData;
        return _screenPinned ? screen : Qt.application.screens[QsNative.cursorScreenIndex()];
    }

    function _place() {
        const s = _targetScreen();
        if (!s) return;
        const a = anchors, m = margins;
        const w = (a.left && a.right) ? s.width - m.left - m.right
                : (implicitWidth > 0 ? implicitWidth : width);
        const h = (a.top && a.bottom) ? s.height - m.top - m.bottom
                : (implicitHeight > 0 ? implicitHeight : height);
        x = a.left ? s.virtualX + m.left
          : a.right ? s.virtualX + s.width - m.right - w
          : s.virtualX + (s.width - w) / 2;
        y = a.top ? s.virtualY + m.top
          : a.bottom ? s.virtualY + s.height - m.bottom - h
          : s.virtualY + (s.height - h) / 2;
        width = w;
        height = h;
    }

    function _wantsFocus() {
        return WlrLayershell.keyboardFocus !== WlrKeyboardFocus.None || focusable;
    }

    function _configure() {
        QsMacWindow.configure(win, WlrLayershell.layer, _wantsFocus() ? WlrLayershell.keyboardFocus : WlrKeyboardFocus.None);
    }

    // Qt resets the NSWindow level whenever it shows the window, so the
    // native setup runs a tick after the show, then the frame is re-applied
    // (AppKit may have pushed a low-level window below the menu bar).
    function _shown() {
        if (!visible) return;
        _configure();
        _place();
        if (_wantsFocus()) { QsMacWindow.activate(win); _tookFocus = true; }
    }

    Component.onCompleted: {
        _completed = true;
        _place();
        if (mask) QsMacWindow.setInputMask(win, mask);
        if (visible) Qt.callLater(_shown);
    }

    onVisibleChanged: {
        if (!_completed) return;
        if (visible) {
            _place();
            Qt.callLater(_shown);
        } else if (_tookFocus) {
            _tookFocus = false;
            QsMacWindow.restoreFocus();
        }
    }

    onMaskChanged: if (_completed) QsMacWindow.setInputMask(win, mask)

    onImplicitWidthChanged: if (_completed) _place()
    onImplicitHeightChanged: if (_completed) _place()

    Connections {
        target: win.anchors
        function onTopChanged() { if (win._completed) win._place(); }
        function onBottomChanged() { if (win._completed) win._place(); }
        function onLeftChanged() { if (win._completed) win._place(); }
        function onRightChanged() { if (win._completed) win._place(); }
    }
    Connections {
        target: win.margins
        function onTopChanged() { if (win._completed) win._place(); }
        function onBottomChanged() { if (win._completed) win._place(); }
        function onLeftChanged() { if (win._completed) win._place(); }
        function onRightChanged() { if (win._completed) win._place(); }
    }
    // An auto-hidden native menu bar slides in over top-edge panels; step
    // aside while it's down so it can be read (and clicked).
    readonly property bool _yieldsToMenuBar: anchors.top && !anchors.bottom && WlrLayershell.layer >= WlrLayer.Top
    Connections {
        target: QsMacMenuBar
        function onRevealedChanged() {
            if (win._completed && win.visible && win._yieldsToMenuBar)
                QsMacWindow.setSuppressed(win, QsMacMenuBar.revealed);
        }
    }

    Connections {
        target: win.WlrLayershell
        function onLayerChanged() { if (win._completed) win._configure(); }
        function onKeyboardFocusChanged() { if (win._completed) win._configure(); }
    }
}
