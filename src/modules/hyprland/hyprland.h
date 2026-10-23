#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

// Quickshell.Hyprland on macOS, backed by yabai: yabai spaces are the
// workspaces (id = space index), displays are the monitors, windows are the
// toplevels. Needs yabai running.

class HyprlandToplevel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("HyprlandToplevel comes from Hyprland.")
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString appId READ appId NOTIFY changed)
    Q_PROPERTY(bool activated READ activated NOTIFY changed)

public:
    HyprlandToplevel(int id, QObject* parent) : QObject(parent), mId(id) {}
    QString address() const { return QString("0x%1").arg(mId, 0, 16); }
    QString title() const { return mTitle; }
    QString appId() const { return mApp; }
    bool activated() const { return mActivated; }
    void set(const QString& title, const QString& app, bool activated);

signals:
    void changed();

private:
    int mId;
    QString mTitle, mApp;
    bool mActivated = false;
};

class HyprlandMonitor;

class HyprlandWorkspace : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("HyprlandWorkspace comes from Hyprland.")
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool focused READ focused NOTIFY changed)
    Q_PROPERTY(bool urgent READ urgent CONSTANT)
    Q_PROPERTY(HyprlandMonitor* monitor READ monitor NOTIFY changed)
    Q_PROPERTY(ObjectModel* toplevels READ toplevels CONSTANT)

public:
    HyprlandWorkspace(int id, QObject* parent);
    int id() const { return mId; }
    QString name() const { return QString::number(mId); }
    bool active() const { return mActive; }
    bool focused() const { return mFocused; }
    bool urgent() const { return false; }
    HyprlandMonitor* monitor() const { return mMonitor; }
    ObjectModel* toplevels() const { return mToplevels; }
    Q_INVOKABLE void activate();

    void set(bool active, bool focused, HyprlandMonitor* monitor);

signals:
    void changed();

private:
    int mId;
    bool mActive = false;
    bool mFocused = false;
    HyprlandMonitor* mMonitor = nullptr;
    ObjectModel* mToplevels;
};

class HyprlandMonitor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("HyprlandMonitor comes from Hyprland.")
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(bool focused READ focused NOTIFY changed)
    Q_PROPERTY(HyprlandWorkspace* activeWorkspace READ activeWorkspace NOTIFY changed)

public:
    HyprlandMonitor(int id, QObject* parent) : QObject(parent), mId(id) {}
    int id() const { return mId; }
    QString name() const { return mName; }
    bool focused() const { return mFocused; }
    HyprlandWorkspace* activeWorkspace() const { return mActive; }
    void set(const QString& name, bool focused, HyprlandWorkspace* active);

signals:
    void changed();

private:
    int mId;
    QString mName;
    bool mFocused = false;
    HyprlandWorkspace* mActive = nullptr;
};

class Hyprland : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(ObjectModel* workspaces READ workspaces CONSTANT)
    Q_PROPERTY(ObjectModel* monitors READ monitors CONSTANT)
    Q_PROPERTY(ObjectModel* toplevels READ toplevels CONSTANT)
    Q_PROPERTY(HyprlandWorkspace* focusedWorkspace READ focusedWorkspace NOTIFY focusedWorkspaceChanged)
    Q_PROPERTY(HyprlandMonitor* focusedMonitor READ focusedMonitor NOTIFY focusedMonitorChanged)

public:
    explicit Hyprland(QObject* parent = nullptr);

    ObjectModel* workspaces() const { return mWorkspaces; }
    ObjectModel* monitors() const { return mMonitors; }
    ObjectModel* toplevels() const { return mToplevels; }
    HyprlandWorkspace* focusedWorkspace() const { return mFocusedWorkspace; }
    HyprlandMonitor* focusedMonitor() const { return mFocusedMonitor; }

    // Hyprland dispatchers mapped onto yabai: workspace switching and moving
    // the active window, in both "workspace 3" and Lua
    // `hl.dsp.focus({ workspace = 3 })` forms. Others are logged and ignored.
    Q_INVOKABLE void dispatch(const QString& request);
    Q_INVOKABLE void refreshWorkspaces() { refresh(); }
    Q_INVOKABLE void refreshMonitors() { refresh(); }
    Q_INVOKABLE void refreshToplevels() { refresh(); }

signals:
    void focusedWorkspaceChanged();
    void focusedMonitorChanged();
    void rawEvent(QVariant event);

private:
    void refresh();
    void apply(const QByteArray& spaces, const QByteArray& displays, const QByteArray& windows);
    void installSignals();

    ObjectModel* mWorkspaces;
    ObjectModel* mMonitors;
    ObjectModel* mToplevels;
    QHash<int, HyprlandWorkspace*> mWorkspaceById;
    QHash<int, HyprlandMonitor*> mMonitorById;
    QHash<int, HyprlandToplevel*> mToplevelById;
    HyprlandWorkspace* mFocusedWorkspace = nullptr;
    HyprlandMonitor* mFocusedMonitor = nullptr;
    QString mEventFile;
    QFileSystemWatcher mWatcher;
    QTimer mDebounce;
    QTimer mPoll;
    bool mRefreshing = false;
    bool mRefreshAgain = false;
};
