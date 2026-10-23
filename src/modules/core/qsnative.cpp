#include "qsnative.h"

#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QMetaObject>
#include <QScreen>
#include <QUrl>

#include "iconprovider.h"
#include "paths.h"
#include "process.h"

QString QsNative::shellDir() const { return QsPaths::shellDir(); }
QString QsNative::stateDir() const { return QsPaths::stateDir(); }
QString QsNative::cacheDir() const { return QsPaths::cacheDir(); }
QString QsNative::dataDir() const { return QsPaths::dataDir(); }
qint64 QsNative::processId() const { return QCoreApplication::applicationPid(); }

QVariant QsNative::env(const QString& name) const {
    if (!qEnvironmentVariableIsSet(name.toUtf8().constData())) return {};
    return qEnvironmentVariable(name.toUtf8().constData());
}

// iconPath(name), iconPath(name, check: bool) or iconPath(name, fallback: string).
QString QsNative::iconPath(const QString& name, const QVariant& checkOrFallback) const {
    const QString resolved = IconProvider::resolve(name);
    if (!resolved.isEmpty()) return "image://icon/" + QString::fromUtf8(QUrl::toPercentEncoding(resolved));
    if (checkOrFallback.typeId() == QMetaType::QString) {
        const QString fallback = checkOrFallback.toString();
        if (!fallback.isEmpty() && fallback != name) return iconPath(fallback, true);
    }
    if (checkOrFallback.typeId() == QMetaType::Bool && checkOrFallback.toBool()) return {};
    return "image://icon/" + QString::fromUtf8(QUrl::toPercentEncoding(name));
}

void QsNative::execDetached(const QVariant& context) const {
    if (context.typeId() == QMetaType::QVariantMap) {
        const QVariantMap m = context.toMap();
        Process::launchDetached(m.value("command").toStringList(), m.value("environment").toMap(),
                                m.value("clearEnvironment").toBool(), m.value("workingDirectory").toString());
    } else {
        Process::launchDetached(context.toStringList(), {}, false, {});
    }
}

int QsNative::cursorScreenIndex() const {
    const auto screens = QGuiApplication::screens();
    QScreen* s = QGuiApplication::screenAt(QCursor::pos());
    const int i = screens.indexOf(s);
    return i < 0 ? 0 : i;
}

void QsNative::reload(bool) const {
    // Queued: the reload destroys the QML tree this call came from.
    QMetaObject::invokeMethod(QCoreApplication::instance(), [] { QsPaths::requestReload(); }, Qt::QueuedConnection);
}
