import QtQuick

// Wayland screencopy has no counterpart for Laumei yet: an empty view.
Item {
    property var captureSource: null
    property bool live: false
    property bool paintCursor: false
    readonly property bool hasContent: false
    readonly property size sourceSize: Qt.size(0, 0)
    function captureFrame() {}
}
