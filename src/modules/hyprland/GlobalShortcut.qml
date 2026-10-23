import QtQml

// Hyprland global shortcuts can't be registered on macOS from here. Bind the
// key in skhd (or similar) to `laumei ipc call <target> <function>` and give
// the shell an IpcHandler instead; pressed/released never fire.
QtObject {
    property string name
    property string description
    property string appid: "quickshell"
    property string triggerDescription
    signal pressed()
    signal released()
}
