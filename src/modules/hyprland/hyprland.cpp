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
#include <QStandardPaths>

#include "displaynames.h"
#include "paths.h"

namespace {

// yabai events that can change spaces, displays or the windows on them.
const char* const kYabaiEvents[] = {
    "space_changed",      "space_created",          "space_destroyed",      "display_changed",
    "display_added",      "display_removed",        "window_created",       "window_destroyed",
    "window_focused",     "window_minimized",       "window_deminimized",   "application_launched",
    "application_terminated", "application_hidden", "application_visible",
};

// Separates the outputs of the queries run in one shell.
constexpr char kSep = '\x1e';

QProcessEnvironment childEnv() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", QsPaths::childPath());
    return env;
}

void runDetached(const QString& script, const QStringList& args = {}) {
    QProcess p;
    p.setProcessEnvironment(childEnv());
    p.setProgram("/bin/sh");
    p.setArguments(QStringList{"-c", script, "sh"} + args);
    p.startDetached();
}

bool onPath(const QString& tool) {
    return !QStandardPaths::findExecutable(tool, QsPaths::childPath().split(':')).isEmpty();
}

} // namespace

void HyprlandToplevel::set(const QString& title, const QString& app, bool activated) {
    if (title == mTitle && app == mApp && activated == mActivated) return;
    mTitle = title;
    mApp = app;
    mActivated = activated;
    emit changed();
}

HyprlandWorkspace::HyprlandWorkspace(int id, const QString& name, QObject* parent)
    : QObject(parent), mId(id), mName(name), mToplevels(new ObjectModel(this)) {}

void HyprlandWorkspace::set(bool active, bool focused, HyprlandMonitor* monitor) {
    if (active == mActive && focused == mFocused && monitor == mMonitor) return;
    mActive = active;
    mFocused = focused;
    mMonitor = monitor;
    emit changed();
}

void HyprlandWorkspace::activate() {
    if (auto* h = qobject_cast<Hyprland*>(parent())) h->dispatch("workspace " + mName);
}

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
    const QString wm = qEnvironmentVariable("LAUMEI_WM").toLower();
    if (wm == "aerospace" || (wm != "yabai" && !onPath("yabai") && onPath("aerospace")))
        mBackend = Backend::AeroSpace;

    mDebounce.setSingleShot(true);
    mDebounce.setInterval(40);
    connect(&mDebounce, &QTimer::timeout, this, &Hyprland::refresh);
    connect(&mPoll, &QTimer::timeout, this, &Hyprland::refresh);

    if (mBackend == Backend::Yabai) {
        // yabai signals touch this file; the watcher turns that into a refresh.
        mEventFile = QDir::tempPath() + "/laumei/yabai-events";
        QDir().mkpath(QDir::tempPath() + "/laumei");
        { QFile f(mEventFile); f.open(QIODevice::WriteOnly | QIODevice::Append); }
        connect(&mWatcher, &QFileSystemWatcher::fileChanged, this, [this] {
            if (!mWatcher.files().contains(mEventFile)) mWatcher.addPath(mEventFile);
            mDebounce.start();
        });
        mWatcher.addPath(mEventFile);
        installSignals();
        // Safety net for changes yabai has no event for (moving a window
        // between spaces without following it).
        mPoll.setInterval(5000);
    } else {
        // AeroSpace can only report changes through callbacks in its own
        // config; its CLI is cheap enough to poll instead.
        mPoll.setInterval(1000);
    }
    mPoll.start();
    refresh();
}

void Hyprland::installSignals() {
    // Labelled, so re-adding on every start replaces rather than duplicates.
    const QString action = QString("echo x > '%1'").arg(mEventFile);
    for (const char* ev : kYabaiEvents) {
        auto* p = new QProcess(this);
        p->setProcessEnvironment(childEnv());
        connect(p, &QProcess::finished, p, &QObject::deleteLater);
        p->start("/bin/sh", {"-c", "exec yabai \"$@\"", "yabai", "-m", "signal", "--add",
                             QString("label=laumei_%1").arg(ev), QString("event=%1").arg(ev), "action=" + action});
    }
}

void Hyprland::refresh() {
    if (mRefreshing) {
        mRefreshAgain = true;
        return;
    }
    mRefreshing = true;

    const QString script = mBackend == Backend::Yabai
        ? "yabai -m query --spaces && printf '\\036' && yabai -m query --displays && printf '\\036' && "
          "yabai -m query --windows"
        : "aerospace list-monitors --format '%{monitor-id}|%{monitor-name}' && printf '\\036' && "
          "aerospace list-monitors --focused --format '%{monitor-id}' && printf '\\036' && "
          "aerospace list-workspaces --all --format "
          "'%{workspace}|%{monitor-id}|%{workspace-is-visible}|%{workspace-is-focused}' && printf '\\036' && "
          "aerospace list-windows --all --format '%{window-id}|%{workspace}|%{app-name}|%{window-title}' && "
          "printf '\\036' && aerospace list-windows --focused --format '%{window-id}' 2>/dev/null; true";

    auto* p = new QProcess(this);
    p->setProcessEnvironment(childEnv());
    connect(p, &QProcess::finished, this, [this, p](int code) {
        const QByteArray out = p->readAllStandardOutput();
        p->deleteLater();
        mRefreshing = false;
        if (code == 0) apply(mBackend == Backend::Yabai ? parseYabai(out) : parseAeroSpace(out));
        if (mRefreshAgain) {
            mRefreshAgain = false;
            refresh();
        }
    });
    p->start("/bin/sh", {"-c", script});
}

Hyprland::Snapshot Hyprland::parseYabai(const QByteArray& out) const {
    Snapshot snap;
    const QList<QByteArray> parts = out.split(kSep);
    if (parts.size() != 3) return snap;

    for (const auto& v : QJsonDocument::fromJson(parts[1]).array()) {
        const QJsonObject d = v.toObject();
        // Qt (and so Quickshell.screens) names screens like macOS does.
        snap.monitors.append({d.value("index").toInt(), displayName(d.value("id").toInteger()),
                              d.value("has-focus").toBool()});
    }
    // A space labelled ws<N> is workspace N wherever it sits; yabai's own
    // indexes run display by display, so "2 on the external monitor" needs one.
    static const QRegularExpression wsLabel(R"re(^ws(\d+)$)re");
    QHash<int, int> idOfIndex;
    for (const auto& v : QJsonDocument::fromJson(parts[0]).array()) {
        const QJsonObject s = v.toObject();
        const int index = s.value("index").toInt();
        const auto m = wsLabel.match(s.value("label").toString());
        const int id = m.hasMatch() ? m.captured(1).toInt() : index;
        idOfIndex[index] = id;
        snap.workspaces.append({id, QString::number(id), s.value("display").toInt(), s.value("is-visible").toBool(),
                                s.value("has-focus").toBool()});
    }
    for (const auto& v : QJsonDocument::fromJson(parts[2]).array()) {
        const QJsonObject w = v.toObject();
        if (w.value("is-minimized").toBool() || w.value("is-hidden").toBool()) continue;
        if (w.value("subrole").toString() != "AXStandardWindow") continue;
        if (w.value("pid").toInteger() == QCoreApplication::applicationPid()) continue; // our own panels
        const int space = w.value("space").toInt();
        snap.windows.append({w.value("id").toInt(), idOfIndex.value(space, space), w.value("app").toString(),
                             w.value("title").toString(), w.value("has-focus").toBool()});
    }
    snap.valid = true;
    return snap;
}

Hyprland::Snapshot Hyprland::parseAeroSpace(const QByteArray& out) const {
    Snapshot snap;
    const QList<QByteArray> parts = out.split(kSep);
    if (parts.size() < 5) return snap;
    auto lines = [](const QByteArray& b) { return QString::fromUtf8(b).split('\n', Qt::SkipEmptyParts); };

    const int focusedMonitor = QString::fromUtf8(parts[1]).trimmed().toInt();
    for (const QString& l : lines(parts[0])) {
        const int id = l.section('|', 0, 0).toInt();
        snap.monitors.append({id, l.section('|', 1), id == focusedMonitor});
    }

    // Workspace names are free-form; numeric ones keep their number as id
    // (configs index workspaces 1..N), the rest are numbered after them.
    QHash<QString, int> idOf;
    int nextId = 1000;
    for (const QString& l : lines(parts[2])) {
        const QString name = l.section('|', 0, 0);
        bool numeric;
        const int n = name.toInt(&numeric);
        const int id = numeric ? n : nextId++;
        idOf[name] = id;
        snap.workspaces.append({id, name, l.section('|', 1, 1).toInt(), l.section('|', 2, 2) == "true",
                                l.section('|', 3, 3) == "true"});
    }

    const int focusedWindow = QString::fromUtf8(parts[4]).trimmed().toInt();
    for (const QString& l : lines(parts[3])) {
        const int id = l.section('|', 0, 0).toInt();
        snap.windows.append({id, idOf.value(l.section('|', 1, 1), -1), l.section('|', 2, 2), l.section('|', 3),
                             id == focusedWindow});
    }
    snap.valid = !snap.monitors.isEmpty();
    return snap;
}

void Hyprland::apply(const Snapshot& snap) {
    if (!snap.valid) return;
    // Monitors.
    QSet<int> seenMonitors;
    HyprlandMonitor* focusedMonitor = nullptr;
    for (const auto& m : snap.monitors) {
        seenMonitors.insert(m.id);
        auto*& mon = mMonitorById[m.id];
        if (!mon) {
            mon = new HyprlandMonitor(m.id, this);
            mMonitors->append(mon);
        }
        if (m.focused) focusedMonitor = mon;
    }

    // Toplevels.
    QHash<int, QList<HyprlandToplevel*>> byWorkspace;
    QSet<int> seenToplevels;
    for (const auto& w : snap.windows) {
        seenToplevels.insert(w.id);
        auto*& t = mToplevelById[w.id];
        if (!t) {
            t = new HyprlandToplevel(w.id, this);
            mToplevels->append(t);
        }
        t->set(w.title, w.app, w.focused);
        byWorkspace[w.workspace].append(t);
    }
    for (auto it = mToplevelById.begin(); it != mToplevelById.end();) {
        if (seenToplevels.contains(it.key())) { ++it; continue; }
        mToplevels->remove(it.value());
        for (auto* ws : std::as_const(mWorkspaceById)) ws->toplevels()->remove(it.value());
        it.value()->deleteLater();
        it = mToplevelById.erase(it);
    }

    // Workspaces: like Hyprland, only those with windows or on screen exist.
    QSet<int> seenWorkspaces;
    HyprlandWorkspace* focused = nullptr;
    QHash<int, HyprlandWorkspace*> visibleOnMonitor;
    for (const auto& s : snap.workspaces) {
        if (!s.visible && byWorkspace.value(s.id).isEmpty()) continue;
        seenWorkspaces.insert(s.id);
        auto*& ws = mWorkspaceById[s.id];
        if (!ws) {
            ws = new HyprlandWorkspace(s.id, s.name, this);
            mWorkspaces->append(ws);
        }
        ws->set(s.visible, s.focused, mMonitorById.value(s.monitor));
        if (s.visible) visibleOnMonitor[s.monitor] = ws;
        if (s.focused) focused = ws;

        const QList<HyprlandToplevel*> want = byWorkspace.value(s.id);
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

    for (const auto& m : snap.monitors) mMonitorById[m.id]->set(m.name, m.focused, visibleOnMonitor.value(m.id));
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
    static const QRegularExpression luaTarget(R"re(workspace\s*=\s*"?([^",}\s]+)"?)re");
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
        qWarning().noquote() << "Hyprland.dispatch: no equivalent for" << request;
        return;
    }

    // "e+1"/"e-1" (and "+1"/"-1"): next/previous workspace.
    QString sel = target;
    if (target.endsWith("+1")) sel = "next";
    else if (target.endsWith("-1")) sel = "prev";

    if (mBackend == Backend::Yabai) {
        // Workspace N is the space labelled wsN if there is one, else index N.
        const QString space = R"(s="$1"; case "$s" in *[!0-9]*) ;; *) yabai -m query --spaces --space "ws$s" >/dev/null 2>&1 && s="ws$s";; esac; )";
        if (verb == "workspace") runDetached(space + "yabai -m space --focus \"$s\"", {sel});
        else if (verb == "movetoworkspacesilent") runDetached(space + "yabai -m window --space \"$s\"", {sel});
        // Move, then follow; one shell so the focus waits for the move.
        else runDetached(space + "yabai -m window --space \"$s\" && yabai -m space --focus \"$s\"", {sel});
    } else {
        if (verb == "workspace") runDetached("aerospace workspace \"$1\"", {sel});
        else if (verb == "movetoworkspacesilent") runDetached("aerospace move-node-to-workspace \"$1\"", {sel});
        else runDetached("aerospace move-node-to-workspace --focus-follows-window \"$1\"", {sel});
    }
    mDebounce.start();
}
