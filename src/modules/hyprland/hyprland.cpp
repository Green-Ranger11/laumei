#include "hyprland.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include "paths.h"

namespace {

// yabai events that can change spaces, displays or the windows on them.
const char* const kEvents[] = {
    "space_changed",      "space_created",          "space_destroyed",      "display_changed",
    "display_added",      "display_removed",        "window_created",       "window_destroyed",
    "window_focused",     "window_minimized",       "window_deminimized",   "application_launched",
    "application_terminated", "application_hidden", "application_visible",
};

QProcess* yabai(QObject* parent, const QStringList& args) {
    auto* p = new QProcess(parent);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", QsPaths::childPath());
    p->setProcessEnvironment(env);
    p->setProgram("/bin/sh");
    p->setArguments({"-c", "exec yabai \"$@\"", "yabai"});
    p->setArguments(p->arguments() + args);
    return p;
}

void yabaiDetached(const QStringList& args) {
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", QsPaths::childPath());
    p.setProcessEnvironment(env);
    p.setProgram("/bin/sh");
    p.setArguments(QStringList{"-c", "yabai \"$@\"", "yabai"} + args);
    p.startDetached();
}

} // namespace

void HyprlandToplevel::set(const QString& title, const QString& app, bool activated) {
    if (title == mTitle && app == mApp && activated == mActivated) return;
    mTitle = title;
    mApp = app;
    mActivated = activated;
    emit changed();
}

HyprlandWorkspace::HyprlandWorkspace(int id, QObject* parent)
    : QObject(parent), mId(id), mToplevels(new ObjectModel(this)) {}

void HyprlandWorkspace::set(bool active, bool focused, HyprlandMonitor* monitor) {
    if (active == mActive && focused == mFocused && monitor == mMonitor) return;
    mActive = active;
    mFocused = focused;
    mMonitor = monitor;
    emit changed();
}

void HyprlandWorkspace::activate() { yabaiDetached({"-m", "space", "--focus", QString::number(mId)}); }

void HyprlandMonitor::set(const QString& name, bool focused, HyprlandWorkspace* active) {
    if (name == mName && focused == mFocused && active == mActive) return;
    mName = name;
    mFocused = focused;
    mActive = active;
    emit changed();
}

Hyprland::Hyprland(QObject* parent)
    : QObject(parent), mWorkspaces(new ObjectModel(this)), mMonitors(new ObjectModel(this)),
      mToplevels(new ObjectModel(this)) {
    mEventFile = QDir::tempPath() + "/laumei/yabai-events";
    QDir().mkpath(QDir::tempPath() + "/laumei");
    { QFile f(mEventFile); f.open(QIODevice::WriteOnly | QIODevice::Append); }

    mDebounce.setSingleShot(true);
    mDebounce.setInterval(40);
    connect(&mDebounce, &QTimer::timeout, this, &Hyprland::refresh);
    connect(&mWatcher, &QFileSystemWatcher::fileChanged, this, [this] {
        if (!mWatcher.files().contains(mEventFile)) mWatcher.addPath(mEventFile);
        mDebounce.start();
    });
    mWatcher.addPath(mEventFile);

    // Safety net for changes yabai has no event for (moving a window between
    // spaces without following it).
    mPoll.setInterval(5000);
    connect(&mPoll, &QTimer::timeout, this, &Hyprland::refresh);
    mPoll.start();

    installSignals();
    refresh();
}

void Hyprland::installSignals() {
    // Labelled, so re-adding on every start replaces rather than duplicates.
    const QString action = QString("echo x > '%1'").arg(mEventFile);
    for (const char* ev : kEvents) {
        auto* p = yabai(this, {"-m", "signal", "--add", QString("label=laumei_%1").arg(ev),
                               QString("event=%1").arg(ev), "action=" + action});
        connect(p, &QProcess::finished, p, &QObject::deleteLater);
        p->start();
    }
}

void Hyprland::refresh() {
    if (mRefreshing) {
        mRefreshAgain = true;
        return;
    }
    mRefreshing = true;

    auto* p = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", QsPaths::childPath());
    p->setProcessEnvironment(env);
    connect(p, &QProcess::finished, this, [this, p](int code) {
        const QByteArray out = p->readAllStandardOutput();
        p->deleteLater();
        mRefreshing = false;
        if (code == 0) {
            const QList<QByteArray> parts = out.split('\x1e');
            if (parts.size() == 3) apply(parts[0], parts[1], parts[2]);
        }
        if (mRefreshAgain) {
            mRefreshAgain = false;
            refresh();
        }
    });
    p->start("/bin/sh", {"-c", "yabai -m query --spaces && printf '\\036' && yabai -m query --displays "
                               "&& printf '\\036' && yabai -m query --windows"});
}

void Hyprland::apply(const QByteArray& spacesJson, const QByteArray& displaysJson, const QByteArray& windowsJson) {
    const QJsonArray spaces = QJsonDocument::fromJson(spacesJson).array();
    const QJsonArray displays = QJsonDocument::fromJson(displaysJson).array();
    const QJsonArray windows = QJsonDocument::fromJson(windowsJson).array();

    // Monitors.
    QSet<int> seenMonitors;
    HyprlandMonitor* focusedMonitor = nullptr;
    for (const auto& v : displays) {
        const QJsonObject d = v.toObject();
        const int id = d.value("index").toInt();
        seenMonitors.insert(id);
        auto*& m = mMonitorById[id];
        if (!m) {
            m = new HyprlandMonitor(id, this);
            mMonitors->append(m);
        }
        if (d.value("has-focus").toBool()) focusedMonitor = m;
    }

    // Toplevels: real windows only (not minimized/hidden, not our own).
    QHash<int, QList<HyprlandToplevel*>> bySpace;
    QSet<int> seenToplevels;
    for (const auto& v : windows) {
        const QJsonObject w = v.toObject();
        if (w.value("is-minimized").toBool() || w.value("is-hidden").toBool()) continue;
        if (w.value("pid").toInteger() == QCoreApplication::applicationPid()) continue; // our own panels
        if (w.value("subrole").toString() != "AXStandardWindow") continue;
        const int id = w.value("id").toInt();
        seenToplevels.insert(id);
        auto*& t = mToplevelById[id];
        if (!t) {
            t = new HyprlandToplevel(id, this);
            mToplevels->append(t);
        }
        t->set(w.value("title").toString(), w.value("app").toString(), w.value("has-focus").toBool());
        bySpace[w.value("space").toInt()].append(t);
    }
    for (auto it = mToplevelById.begin(); it != mToplevelById.end();) {
        if (seenToplevels.contains(it.key())) { ++it; continue; }
        mToplevels->remove(it.value());
        for (auto* ws : std::as_const(mWorkspaceById)) ws->toplevels()->remove(it.value());
        it.value()->deleteLater();
        it = mToplevelById.erase(it);
    }

    // Workspaces: like Hyprland, only spaces with windows or on screen exist.
    QSet<int> seenWorkspaces;
    HyprlandWorkspace* focused = nullptr;
    QHash<int, HyprlandWorkspace*> visibleOnDisplay;
    for (const auto& v : spaces) {
        const QJsonObject s = v.toObject();
        const int id = s.value("index").toInt();
        const bool visible = s.value("is-visible").toBool();
        const bool hasFocus = s.value("has-focus").toBool();
        if (!visible && bySpace.value(id).isEmpty()) continue;
        seenWorkspaces.insert(id);
        auto*& ws = mWorkspaceById[id];
        if (!ws) {
            ws = new HyprlandWorkspace(id, this);
            // Keep the model in id order, like Hyprland's.
            mWorkspaces->append(ws);
        }
        HyprlandMonitor* mon = mMonitorById.value(s.value("display").toInt());
        ws->set(visible, hasFocus, mon);
        if (visible) visibleOnDisplay[s.value("display").toInt()] = ws;
        if (hasFocus) focused = ws;

        const QList<HyprlandToplevel*> want = bySpace.value(id);
        for (QObject* o : ws->toplevels()->values())
            if (!want.contains(static_cast<HyprlandToplevel*>(o))) ws->toplevels()->remove(o);
        for (HyprlandToplevel* t : want)
            if (!ws->toplevels()->contains(t)) ws->toplevels()->append(t);
    }
    for (auto it = mWorkspaceById.begin(); it != mWorkspaceById.end();) {
        if (seenWorkspaces.contains(it.key())) { ++it; continue; }
        mWorkspaces->remove(it.value());
        if (mFocusedWorkspace == it.value()) mFocusedWorkspace = nullptr;
        it.value()->deleteLater();
        it = mWorkspaceById.erase(it);
    }

    for (const auto& v : displays) {
        const QJsonObject d = v.toObject();
        const int id = d.value("index").toInt();
        mMonitorById[id]->set(QString("Display %1").arg(id), d.value("has-focus").toBool(), visibleOnDisplay.value(id));
    }
    for (auto it = mMonitorById.begin(); it != mMonitorById.end();) {
        if (seenMonitors.contains(it.key())) { ++it; continue; }
        mMonitors->remove(it.value());
        if (mFocusedMonitor == it.value()) mFocusedMonitor = nullptr;
        it.value()->deleteLater();
        it = mMonitorById.erase(it);
    }

    if (focused != mFocusedWorkspace) {
        mFocusedWorkspace = focused;
        emit focusedWorkspaceChanged();
    }
    if (focusedMonitor != mFocusedMonitor) {
        mFocusedMonitor = focusedMonitor;
        emit focusedMonitorChanged();
    }
}

void Hyprland::dispatch(const QString& request) {
    static const QRegularExpression luaTarget(R"re(workspace\s*=\s*"?([+-]?[\w+-]+)"?)re");
    static const QRegularExpression classic(R"re(^\s*(workspace|movetoworkspace|movetoworkspacesilent)\s+(\S+))re");

    QString verb, target;
    if (auto m = classic.match(request); m.hasMatch()) {
        verb = m.captured(1);
        target = m.captured(2);
    } else if (auto m2 = luaTarget.match(request); m2.hasMatch()) {
        target = m2.captured(1);
        verb = request.contains("window.move") ? (request.contains("silent") ? "movetoworkspacesilent" : "movetoworkspace")
                                               : "workspace";
    } else {
        qWarning().noquote() << "Hyprland.dispatch: no yabai equivalent for" << request;
        return;
    }

    // "e+1"/"e-1" (and "+1"/"-1"): next/previous space.
    QString sel = target;
    if (target.endsWith("+1")) sel = "next";
    else if (target.endsWith("-1")) sel = "prev";

    if (verb == "workspace") {
        yabaiDetached({"-m", "space", "--focus", sel});
    } else if (verb == "movetoworkspacesilent") {
        yabaiDetached({"-m", "window", "--space", sel});
    } else {
        // Move, then follow; in one shell so the focus waits for the move.
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PATH", QsPaths::childPath());
        p.setProcessEnvironment(env);
        p.setProgram("/bin/sh");
        p.setArguments({"-c", "yabai -m window --space \"$1\" && yabai -m space --focus \"$1\"", "sh", sel});
        p.startDetached();
    }
}
