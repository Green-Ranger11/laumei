#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// Quickshell.Io IpcHandler: exposes the functions declared on it to
// `laumei ipc call <target> <function> [args...]`.
class IpcHandler : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit IpcHandler(QObject* parent = nullptr);
    ~IpcHandler() override;

    QString target() const { return mTarget; }
    void setTarget(const QString& t);
    bool enabled() const { return mEnabled; }
    void setEnabled(bool e);

    // "function name(arg: type, ...): returnType" lines for `laumei ipc show`.
    QStringList describeFunctions() const;

    // Calls `function` with string arguments coerced to its declared parameter
    // types. Returns false (with `error` set) if it can't be called.
    bool call(const QString& function, const QStringList& args, QString* result, QString* error);

    static QList<IpcHandler*> handlers();
    static IpcHandler* find(const QString& target);

signals:
    void targetChanged();
    void enabledChanged();

private:
    QString mTarget;
    bool mEnabled = true;
};
