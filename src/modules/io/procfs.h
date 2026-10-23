#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

// Linux files that configs read for system stats, synthesized from macOS
// APIs so FileView can serve them unchanged:
//   /proc/stat                              (aggregate "cpu" line, in ticks)
//   /proc/meminfo                           (MemTotal, MemFree, MemAvailable)
//   /sys/class/thermal/thermal_zone0/temp   (CPU die temperature, millidegrees)
//   /sys/class/backlight/<any>/{brightness,actual_brightness,max_brightness}
//                                           (built-in display, 0..1000)
//   /etc/hostname
// Returns nothing for any other path.
namespace ProcFs {
std::optional<QByteArray> read(const QString& path);
}
