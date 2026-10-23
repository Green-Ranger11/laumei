import QtQuick

// Never shown on macOS (see WlSessionLock); accepts a lock UI so it loads.
Item {
    property var screen: null
    property color color: "black"
}
