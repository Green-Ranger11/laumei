#pragma once

#include <QString>
#include <QStringList>
#include <functional>

// Process-wide locations, set once by main() before the shell loads.
namespace QsPaths {

// Absolute path of the loaded shell.qml and its directory.
void setShellPath(const QString& shellQml);
QString shellPath();
QString shellDir();

// Stable id for the loaded shell (hash of its path), shared by the state/cache
// dirs and the IPC socket so `laumei -p <dir> ipc` finds the right instance.
QString shellId();

// ~/.local/state/quickshell/by-shell/<id>, like Quickshell on Linux.
QString stateDir();
QString cacheDir();
QString dataDir();

// Directory holding the compat/bin shims (wl-copy, notify-send, ...).
QString compatBinDir();

// PATH for child processes: compat shims first, then the inherited PATH,
// then ~/.local/bin and Homebrew (launchd/skhd give a bare /usr/bin:/bin).
QString childPath();

// Quickshell.reload(): main() installs the handler that rebuilds the shell.
void setReloadHandler(std::function<void()> handler);
void requestReload();

} // namespace QsPaths
