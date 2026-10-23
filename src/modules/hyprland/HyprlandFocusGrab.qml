import QtQml

// Hyprland's focus grab (dismiss popups on outside click) has no macOS
// counterpart for Laumei; `active` never takes effect and cleared never fires.
QtObject {
    property list<QtObject> windows
    property bool active: false
    signal cleared()
}
