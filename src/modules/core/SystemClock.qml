import QtQml

// Quickshell's SystemClock: `date` (and hours/minutes/seconds) updated on
// each boundary of `precision`, not on a free-running interval.
QtObject {
    id: clock

    enum Precision { Hours, Minutes, Seconds }

    property bool enabled: true
    property int precision: SystemClock.Seconds
    property date date: new Date()
    readonly property int hours: date.getHours()
    readonly property int minutes: date.getMinutes()
    readonly property int seconds: date.getSeconds()

    function _tick() {
        const now = new Date();
        const d = new Date(now);
        if (precision <= SystemClock.Minutes) d.setSeconds(0, 0);
        else d.setMilliseconds(0);
        if (precision === SystemClock.Hours) d.setMinutes(0);
        date = d;

        // Wake just after the next boundary.
        const next = new Date(d);
        if (precision === SystemClock.Hours) next.setHours(d.getHours() + 1);
        else if (precision === SystemClock.Minutes) next.setMinutes(d.getMinutes() + 1);
        else next.setSeconds(d.getSeconds() + 1);
        _timer.interval = Math.max(1, next - now + 5);
        _timer.restart();
    }

    property Timer _timer: Timer {
        running: false
        repeat: false
        onTriggered: clock._tick()
    }

    onEnabledChanged: if (enabled) _tick(); else _timer.stop()
    onPrecisionChanged: if (enabled) _tick()
    Component.onCompleted: if (enabled) _tick()
}
