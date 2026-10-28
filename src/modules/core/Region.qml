import QtQml

// Quickshell's Region: an input mask for PanelWindow.mask, evaluated by
// QsMacWindow.setInputMask. Child regions apply in order by their
// `intersection`. Rounded corners are taken as square.
QtObject {
    property var item: null
    property real x
    property real y
    property real width
    property real height
    property int shape: RegionShape.Rect
    property int intersection: Intersection.Combine
    property int radius
    property int topLeftRadius: radius
    property int topRightRadius: radius
    property int bottomLeftRadius: radius
    property int bottomRightRadius: radius
    default property list<QtObject> regions
}
