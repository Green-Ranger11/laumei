// Core Quickshell types: paths, env, SystemClock, Variants, LazyLoader,
// Process + parsers, FileView.
import QtQuick
import Quickshell
import Quickshell.Io
import "../lib"

ShellRoot {
    id: root
    property Check t: Check {}
    property int lazyCreated: 0
    property var lines: []

    property SystemClock clock: SystemClock { precision: SystemClock.Seconds }

    property Variants variants: Variants {
        model: ["a", "b", "c"]
        QtObject { required property string modelData }
    }

    property LazyLoader lazy: LazyLoader {
        active: false
        QtObject { Component.onCompleted: root.lazyCreated++ }
    }

    property Process echo: Process {
        command: ["sh", "-c", "printf 'one\\ntwo\\nthree'"]
        stdout: SplitParser { onRead: line => root.lines = root.lines.concat([line]) }
    }

    property Process collect: Process {
        command: ["echo", "hello"]
        stdout: StdioCollector { id: out }
    }

    property FileView file: FileView { path: Quickshell.cacheDir + "/laumei-test.txt"; printErrors: false }

    Component.onCompleted: {
        t.check("env HOME", (Quickshell.env("HOME") ?? "").length > 0);
        t.check("env missing is null", Quickshell.env("LAUMEI_DEFINITELY_UNSET") === null);
        t.check("stateDir by shell", Quickshell.stateDir.indexOf("/quickshell/by-shell/") !== -1, Quickshell.stateDir);
        t.check("shellDir", Quickshell.shellDir.endsWith("/tests/core"), Quickshell.shellDir);
        t.check("screens", Quickshell.screens.length > 0);
        t.check("SystemClock date", clock.date.getFullYear() >= 2024);
        t.check("Variants instances", variants.instances.length === 3, variants.instances.length);
        t.check("LazyLoader inactive", lazy.item === null && lazyCreated === 0);
        lazy.active = true;
        t.check("LazyLoader active", lazy.item !== null && lazyCreated === 1);
        lazy.active = false;
        t.check("LazyLoader released", lazy.item === null);
        file.setText("written by laumei tests\n");
        t.check("FileView round trip", file.text() === "written by laumei tests\n", file.text());
        echo.running = true;
        collect.running = true;
        later.start();
    }

    property Timer later: Timer {
        interval: 1500
        onTriggered: {
            t.check("SplitParser lines", JSON.stringify(root.lines) === '["one","two","three"]', JSON.stringify(root.lines));
            t.check("StdioCollector text", out.text === "hello\n", JSON.stringify(out.text));
            t.finish();
        }
    }
}
