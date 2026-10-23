import QtQuick

// PopupWindow.anchor: what the popup is placed against. `rect` is relative to
// `item` (or `window`); empty means the whole item. `edges` picks the point
// on that rect, `gravity` the direction the popup grows from it.
QtObject {
    property Item item: null
    property var window: null
    property rect rect: Qt.rect(0, 0, 0, 0)
    property int edges: Edges.Top | Edges.Left
    property int gravity: Edges.Bottom | Edges.Right
    property int adjustment: 0
    readonly property PanelMargins margins: PanelMargins {}
}
