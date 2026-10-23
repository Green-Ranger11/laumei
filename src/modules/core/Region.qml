import QtQml

// Input/visual mask region. Accepted for compatibility; macOS windows here
// always take input over their whole frame.
QtObject {
    property var item: null
    property real x
    property real y
    property real width
    property real height
    property int intersection
    default property list<QtObject> regions
}
