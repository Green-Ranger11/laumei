#include "paths.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

namespace {
QString g_shellPath;
std::function<void()> g_reload;

QString xdg(const char* var, const QString& fallback) {
    const QString v = qEnvironmentVariable(var);
    return v.isEmpty() ? QDir::homePath() + fallback : v;
}

QString byShell(const QString& base) {
    return base + "/quickshell/by-shell/" + QsPaths::shellId();
}
} // namespace

namespace QsPaths {

void setShellPath(const QString& shellQml) { g_shellPath = QFileInfo(shellQml).absoluteFilePath(); }
QString shellPath() { return g_shellPath; }
QString shellDir() { return QFileInfo(g_shellPath).absolutePath(); }

QString shellId() {
    return QString::fromLatin1(
        QCryptographicHash::hash(g_shellPath.toUtf8(), QCryptographicHash::Md5).toHex().left(16));
}

QString stateDir() { return byShell(xdg("XDG_STATE_HOME", "/.local/state")); }
QString cacheDir() { return byShell(xdg("XDG_CACHE_HOME", "/.cache")); }
QString dataDir() { return byShell(xdg("XDG_DATA_HOME", "/.local/share")); }

QString compatBinDir() {
    // Laumei.app/Contents/MacOS/Laumei -> Laumei.app/Contents/Resources/compat/bin.
    const QDir exe(QCoreApplication::applicationDirPath());
    for (const QString& rel : {QStringLiteral("../Resources/compat/bin")}) {
        const QString p = QDir::cleanPath(exe.filePath(rel));
        if (QFileInfo(p).isDir()) return p;
    }
    return {};
}

QString childPath() {
    QStringList parts;
    if (const QString c = compatBinDir(); !c.isEmpty()) parts << c;
    parts << QDir::homePath() + "/.local/bin" << "/opt/homebrew/bin" << "/opt/homebrew/sbin"
          << "/usr/local/bin";
    parts << qEnvironmentVariable("PATH", "/usr/bin:/bin:/usr/sbin:/sbin").split(':');
    parts.removeDuplicates();
    parts.removeAll(QString());
    return parts.join(':');
}

void setReloadHandler(std::function<void()> handler) { g_reload = std::move(handler); }

void requestReload() {
    if (g_reload) g_reload();
}

} // namespace QsPaths
