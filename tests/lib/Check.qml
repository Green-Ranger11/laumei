import QtQml

// Collects test results; finish() prints a summary and exits non-zero on
// any failure, which tests/run.sh reports.
QtObject {
    property int failures: 0
    property int passes: 0

    function check(name, ok, detail) {
        if (ok) { passes++; console.log("  ok   " + name); }
        else { failures++; console.log("  FAIL " + name + (detail !== undefined ? ": " + detail : "")); }
    }

    function finish() {
        console.log(`  ${passes} passed, ${failures} failed`);
        Qt.exit(failures > 0 ? 1 : 0);
    }
}
