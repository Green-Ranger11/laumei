import QtQuick
import QtQuick.Window

// Quickshell's FloatingWindow: an ordinary titled window the window manager
// places. implicitWidth/Height set its size.
Window {
    property real implicitWidth: 0
    property real implicitHeight: 0
    property var minimumSize: undefined
    property var maximumSize: undefined

    visible: true
    width: implicitWidth > 0 ? implicitWidth : 640
    height: implicitHeight > 0 ? implicitHeight : 480
}
