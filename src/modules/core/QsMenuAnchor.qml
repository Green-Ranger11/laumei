import QtQuick

// Opens a platform menu (tray menus on Linux). Nothing on macOS provides those
// menus to Laumei, so open() does nothing and `visible` stays false.
QtObject {
    property var menu: null
    readonly property bool visible: false
    readonly property PopupAnchor anchor: PopupAnchor {}
    signal opened()
    signal closed()
    function open() {}
    function close() {}
}
