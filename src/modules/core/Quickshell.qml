pragma Singleton
import QtQuick

// The `Quickshell` singleton. Paths, env and process helpers come from
// QsNative (C++); screens are Qt's own so they can be assigned to a window's
// `screen` directly.
QtObject {
    // QQuickScreenInfo: name, model, width, height, virtualX, virtualY, ...
    readonly property var screens: Qt.application.screens

    readonly property string shellDir: QsNative.shellDir
    readonly property string configDir: QsNative.shellDir
    readonly property string stateDir: QsNative.stateDir
    readonly property string cacheDir: QsNative.cacheDir
    readonly property string dataDir: QsNative.dataDir
    readonly property real processId: QsNative.processId

    function env(name) { return QsNative.env(name); }
    function shellPath(path) { return QsNative.shellDir + "/" + path; }
    // iconPath(name), iconPath(name, check) or iconPath(name, fallback).
    function iconPath(name, checkOrFallback) {
        return QsNative.iconPath(name, checkOrFallback === undefined ? false : checkOrFallback);
    }
    function execDetached(context) { QsNative.execDetached(context); }
    function reload(hard) { QsNative.reload(!!hard); }
}
