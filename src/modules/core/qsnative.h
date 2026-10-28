#pragma once

#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

// Backing for the QML `Quickshell` singleton (Quickshell.qml), which adds the
// parts that are easier in QML, like `screens`.
class QsNative : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString shellDir READ shellDir CONSTANT)
    Q_PROPERTY(QString stateDir READ stateDir CONSTANT)
    Q_PROPERTY(QString cacheDir READ cacheDir CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
    Q_PROPERTY(qint64 processId READ processId CONSTANT)

public:
    using QObject::QObject;

    QString shellDir() const;
    QString stateDir() const;
    QString cacheDir() const;
    QString dataDir() const;
    qint64 processId() const;

    Q_INVOKABLE QVariant env(const QString& name) const;
    Q_INVOKABLE QString iconPath(const QString& name, const QVariant& checkOrFallback) const;
    Q_INVOKABLE void execDetached(const QVariant& context) const;
    // Index into Qt.application.screens of the screen under the mouse, the
    // closest thing macOS has to the compositor's focused output.
    Q_INVOKABLE int cursorScreenIndex() const;
    Q_INVOKABLE void reload(bool hard) const;
};

// Quickshell's ExclusionMode (PanelWindow.exclusionMode). macOS has no
// exclusive zones, so these only matter for configs that read them back.
class ExclusionMode : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ExclusionMode is an enum.")

public:
    enum Enum { Normal = 0, Ignore = 1, Auto = 2 };
    Q_ENUM(Enum)
};

// Quickshell's Region enums (Region.shape / Region.intersection).
class RegionShape : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("RegionShape is an enum.")

public:
    enum Enum { Rect = 0, Ellipse = 1 };
    Q_ENUM(Enum)
};

class Intersection : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Intersection is an enum.")

public:
    enum Enum { Combine = 0, Subtract = 1, Intersect = 2, Xor = 3 };
    Q_ENUM(Enum)
};
