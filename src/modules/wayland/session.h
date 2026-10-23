#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

// Session-level Wayland protocols, mapped onto what macOS offers.

// Backs WlSessionLock.qml: macOS only trusts its own lock screen.
class QsSessionLock : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;
    // Locks the screen now (as Control Center's "Lock Screen" does).
    Q_INVOKABLE bool lockScreen();
};

// Keeps the display (and so the Mac) awake while enabled, via a power
// assertion. `window` is accepted for compatibility; macOS needs no surface.
class IdleInhibitor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QObject* window MEMBER mWindow NOTIFY windowChanged)

public:
    using QObject::QObject;
    ~IdleInhibitor() override { setEnabled(false); }
    bool enabled() const { return mAssertion != 0; }
    void setEnabled(bool e);

signals:
    void enabledChanged();
    void windowChanged();

private:
    unsigned mAssertion = 0; // IOPMAssertionID
    QObject* mWindow = nullptr;
};

// isIdle once there has been no keyboard/mouse input for `timeout` seconds.
class IdleMonitor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool enabled MEMBER mEnabled NOTIFY enabledChanged)
    Q_PROPERTY(qreal timeout MEMBER mTimeout NOTIFY timeoutChanged)
    Q_PROPERTY(bool respectInhibitors MEMBER mRespectInhibitors NOTIFY respectInhibitorsChanged)
    Q_PROPERTY(bool isIdle READ isIdle NOTIFY isIdleChanged)

public:
    explicit IdleMonitor(QObject* parent = nullptr);
    bool isIdle() const { return mIdle; }

signals:
    void enabledChanged();
    void timeoutChanged();
    void respectInhibitorsChanged();
    void isIdleChanged();

private:
    void poll();

    QTimer mTimer;
    bool mEnabled = true;
    qreal mTimeout = 0;
    bool mRespectInhibitors = true;
    bool mIdle = false;
};

// Foreign toplevel management has no macOS counterpart for other apps'
// windows; the list stays empty.
class ToplevelManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(ObjectModel* toplevels READ toplevels CONSTANT)
    Q_PROPERTY(QObject* activeToplevel READ activeToplevel CONSTANT)

public:
    explicit ToplevelManager(QObject* parent = nullptr) : QObject(parent), mToplevels(new ObjectModel(this)) {}
    ObjectModel* toplevels() const { return mToplevels; }
    QObject* activeToplevel() const { return nullptr; }

private:
    ObjectModel* mToplevels;
};
