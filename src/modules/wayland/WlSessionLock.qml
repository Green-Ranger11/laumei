import QtQml

// WlSessionLock on macOS: setting `locked` hands off to the macOS lock screen,
// the only one macOS trusts, then resets so the next lock request works too.
// `surface` is never shown; a lock UI written for Wayland stays unused.
QtObject {
    id: lock

    property bool locked: false
    readonly property bool secure: false
    default property Component surface

    onLockedChanged: {
        if (!locked) return;
        QsSessionLock.lockScreen();
        Qt.callLater(() => lock.locked = false);
    }
}
