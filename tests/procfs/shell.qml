// Linux files FileView synthesizes on macOS.
import QtQuick
import Quickshell
import Quickshell.Io
import "../lib"

ShellRoot {
    property Check t: Check {}
    property FileView stat: FileView { path: "/proc/stat"; blockLoading: true }
    property FileView meminfo: FileView { path: "/proc/meminfo"; blockLoading: true }
    property FileView hostname: FileView { path: "/etc/hostname"; blockLoading: true }
    property FileView temp: FileView { path: "/sys/class/thermal/thermal_zone0/temp"; blockLoading: true; printErrors: false }
    property FileView maxBrightness: FileView { path: "/sys/class/backlight/any/max_brightness"; blockLoading: true }

    Component.onCompleted: {
        const cpu = stat.text().split("\n")[0].split(/\s+/);
        t.check("/proc/stat cpu line", cpu[0] === "cpu" && cpu.length >= 5 && Number(cpu[4]) > 0, cpu.join(" "));
        const mem = meminfo.text();
        t.check("/proc/meminfo", /^MemTotal:\s+\d+ kB/m.test(mem) && /^MemAvailable:\s+\d+ kB/m.test(mem));
        t.check("/etc/hostname", hostname.text().trim().length > 0);
        // Temperature needs Apple Silicon sensors; tolerate their absence.
        const c = Number(temp.text()) / 1000;
        t.check("thermal zone plausible", temp.text() === "" || (c > 0 && c < 120), temp.text());
        t.check("backlight max", maxBrightness.text().trim() === "1000");
        t.finish();
    }
}
