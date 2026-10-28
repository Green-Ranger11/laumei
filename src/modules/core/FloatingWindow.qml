import QtQuick
import QtQuick.Window

// Quickshell's FloatingWindow: an ordinary titled window the window manager
// places. implicitWidth/Height set its size.
Window {
    property real implicitWidth: 0
    property real implicitHeight: 0
    property var minimumSize: undefined
    property var maximumSize: undefined

    // Top-level even when declared inside an Item: Qt would otherwise make it
    // transient for that Item's window and keep it hidden while there is none.
    transientParent: null
    visible: true
    width: implicitWidth > 0 ? implicitWidth : 640
    height: implicitHeight > 0 ? implicitHeight : 480
}
