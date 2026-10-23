#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Quickshell.Services.Pam, refusing on macOS: it backs custom lock screens,
// and Laumei hands locking to macOS (see WlSessionLock). start() always ends
// in error() -- never completed(Success) -- so nothing can unlock through it.

class PamResult : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("PamResult is an enum.")

public:
    enum Enum { Success = 0, Failed = 1, Error = 2, MaxTries = 3 };
    Q_ENUM(Enum)
};

class PamError : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("PamError is an enum.")

public:
    enum Enum { StartFailed = 0, TryAuthFailed = 1, InternalError = 2 };
    Q_ENUM(Enum)
};

class PamContext : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString config MEMBER mConfig NOTIFY configChanged)
    Q_PROPERTY(QString configDirectory MEMBER mConfigDirectory NOTIFY configChanged)
    Q_PROPERTY(QString user MEMBER mUser NOTIFY configChanged)
    Q_PROPERTY(QString message READ message NOTIFY pamMessage)
    Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY pamMessage)
    Q_PROPERTY(bool responseRequired READ responseRequired NOTIFY pamMessage)
    Q_PROPERTY(bool responseVisible READ responseVisible NOTIFY pamMessage)

public:
    using QObject::QObject;

    bool active() const { return false; }
    void setActive(bool a);
    QString message() const { return mMessage; }
    bool messageIsError() const { return !mMessage.isEmpty(); }
    bool responseRequired() const { return false; }
    bool responseVisible() const { return false; }

    Q_INVOKABLE bool start();
    Q_INVOKABLE void abort() {}
    Q_INVOKABLE void respond(const QString&) {}

signals:
    void activeChanged();
    void configChanged();
    void pamMessage();
    void completed(int result);
    void error(int error);

private:
    QString mConfig, mConfigDirectory, mUser, mMessage;
};
