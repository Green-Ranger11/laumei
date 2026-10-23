import QtQml

// Quickshell's LazyLoader: creates `component` only while `active`. With
// `loading` set (and not active) it is built in the background so turning
// it on later is instant.
QtObject {
    id: loader

    default property Component component
    property Component source: null
    property bool active: false
    property bool loading: false
    readonly property QtObject item: _item

    property QtObject _item: null
    property var _incubator: null

    function _comp() { return source ?? component; }

    function _create(async) {
        if (_item || _incubator || !_comp()) return;
        // Parent to the loader so the object lives in the same tree.
        if (!async) { _item = _comp().createObject(loader); return; }
        _incubator = _comp().incubateObject(loader, {}, Qt.Asynchronous);
        if (_incubator.status === Component.Ready) { _item = _incubator.object; _incubator = null; return; }
        _incubator.onStatusChanged = status => {
            if (status === Component.Ready) _item = _incubator.object;
            _incubator = null;
        };
    }

    function _destroy() {
        if (_incubator) { _incubator.forceCompletion(); _incubator = null; }
        if (_item) { _item.destroy(); _item = null; }
    }

    function _update() {
        if (active) _create(false);
        else if (loading) _create(true);
        else _destroy();
    }

    onActiveChanged: _update()
    onLoadingChanged: _update()
    onComponentChanged: { _destroy(); _update(); }
    onSourceChanged: { _destroy(); _update(); }
    Component.onCompleted: _update()
}
