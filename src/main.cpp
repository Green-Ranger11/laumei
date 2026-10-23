// qs: run a Quickshell config on macOS, or talk to a running one.
//
//   qs [-p <path> | -c <name>] [--replace]       run the shell
//   qs [-p <path> | -c <name>] ipc show          list IPC targets
//   qs [-p <path> | -c <name>] ipc call <target> <function> [args...]
//   qs [-p <path> | -c <name>] kill              stop the running shell
//
// <path> is a shell.qml or a directory holding one; -c <name> means
// ~/.config/quickshell/<name>. Default: ~/.config/quickshell/shell.qml.

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocalServer>
#include <QPointer>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>
#include <cstdio>

#include "iconprovider.h"
#include "ipc.h"
#include "macwindow.h"
#include "paths.h"

Q_IMPORT_QML_PLUGIN(QuickshellPlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_IoPlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_WaylandPlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_WidgetsPlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_BluetoothPlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_NetworkingPlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_HyprlandPlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_Services_PipewirePlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_Services_UPowerPlugin)
Q_IMPORT_QML_PLUGIN(Quickshell_Services_MprisPlugin)

namespace {

void usage() {
    std::fputs("usage: qs [-p <path> | -c <name>] [--replace]\n"
               "       qs [-p <path> | -c <name>] ipc show\n"
               "       qs [-p <path> | -c <name>] ipc call <target> <function> [args...]\n"
               "       qs [-p <path> | -c <name>] kill\n",
               stderr);
}

QString resolveShell(const QString& path, const QString& name) {
    QString p = path;
    if (p.isEmpty() && !name.isEmpty()) p = QDir::homePath() + "/.config/quickshell/" + name;
    if (p.isEmpty()) p = QDir::homePath() + "/.config/quickshell";
    if (p.startsWith("~/")) p = QDir::homePath() + p.mid(1);
    if (QFileInfo(p).isDir()) p += "/shell.qml";
    return QFileInfo(p).absoluteFilePath();
}

// Records every local file the engine loads (QML, qmldir, JS) so the shell
// can reload when any of them changes, wherever it was imported from.
class FileTracker : public QQmlAbstractUrlInterceptor {
public:
    QUrl intercept(const QUrl& url, DataType) override {
        if (url.isLocalFile()) files.insert(url.toLocalFile());
        return url;
    }
    QSet<QString> files;
};

// Owns the engine and the root object; rebuilds both on reload.
class Shell : public QObject {
public:
    explicit Shell(const QString& path) : mPath(path) {
        mDebounce.setSingleShot(true);
        mDebounce.setInterval(100);
        connect(&mDebounce, &QTimer::timeout, this, [this] { load(); });
        connect(&mWatcher, &QFileSystemWatcher::fileChanged, &mDebounce, qOverload<>(&QTimer::start));
        QsPaths::setReloadHandler([this] { load(); });
    }

    bool load() {
        auto* engine = new QQmlEngine(this);
        auto* tracker = new FileTracker;
        engine->addUrlInterceptor(tracker);
        engine->addImageProvider("icon", new IconProvider);

        QQmlComponent component(engine, QUrl::fromLocalFile(mPath));
        QObject* root = component.isError() ? nullptr : component.create();
        // Watch what was loaded even on failure, so fixing the file retries.
        watch(tracker->files);

        if (!root) {
            for (const auto& e : component.errors()) std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
            std::fprintf(stderr, mRoot ? "reload failed; keeping the running config\n" : "failed to load %s\n",
                         qPrintable(mPath));
            engine->removeUrlInterceptor(tracker);
            delete engine;
            delete tracker;
            return false;
        }

        // Swap in the new tree only once it built cleanly.
        if (mRoot) {
            delete mRoot;
            mEngine->removeUrlInterceptor(mTracker);
            mEngine->deleteLater();
            delete mTracker;
            std::fprintf(stderr, "reloaded %s\n", qPrintable(mPath));
        }
        mEngine = engine;
        mTracker = tracker;
        mRoot = root;
        return true;
    }

private:
    void watch(const QSet<QString>& files) {
        if (!mWatcher.files().isEmpty()) mWatcher.removePaths(mWatcher.files());
        for (const QString& f : files)
            if (QFileInfo::exists(f)) mWatcher.addPath(f);
    }

    QString mPath;
    QQmlEngine* mEngine = nullptr;
    FileTracker* mTracker = nullptr;
    QPointer<QObject> mRoot;
    QFileSystemWatcher mWatcher;
    QTimer mDebounce;
};

int runClient(const QStringList& rest) {
    QJsonObject req;
    if (rest.value(0) == "kill") req = {{"cmd", "kill"}};
    else if (rest.value(0) == "ipc" && rest.value(1) == "show") req = {{"cmd", "show"}};
    else if (rest.value(0) == "ipc" && rest.value(1) == "call" && rest.size() >= 4)
        req = {{"cmd", "call"}, {"target", rest[2]}, {"function", rest[3]},
               {"args", QJsonArray::fromStringList(rest.mid(4))}};
    else {
        usage();
        return 2;
    }

    QJsonObject reply;
    if (!Ipc::send(req, &reply)) {
        std::fprintf(stderr, "no running instance for %s\n", qPrintable(QsPaths::shellPath()));
        return 1;
    }
    if (!reply.value("ok").toBool()) {
        std::fprintf(stderr, "%s\n", qPrintable(reply.value("error").toString()));
        return 1;
    }
    if (reply.contains("targets")) {
        for (const auto& t : reply.value("targets").toArray()) {
            std::printf("target %s\n", qPrintable(t.toObject().value("target").toString()));
            for (const auto& f : t.toObject().value("functions").toArray())
                std::printf("  %s\n", qPrintable(f.toString()));
        }
    }
    if (const QString r = reply.value("result").toString(); !r.isEmpty()) std::printf("%s\n", qPrintable(r));
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    QStringList args;
    for (int i = 1; i < argc; i++) args << QString::fromLocal8Bit(argv[i]);

    QString path, name;
    bool replace = false;
    QStringList rest;
    for (int i = 0; i < args.size(); i++) {
        const QString& a = args[i];
        if ((a == "-p" || a == "--path") && i + 1 < args.size()) path = args[++i];
        else if ((a == "-c" || a == "--config") && i + 1 < args.size()) name = args[++i];
        else if (a == "--replace") replace = true;
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else rest << a;
    }
    QsPaths::setShellPath(resolveShell(path, name));

    if (!rest.isEmpty()) {
        QCoreApplication app(argc, argv);
        return runClient(rest);
    }

    if (!QFileInfo::exists(QsPaths::shellPath())) {
        std::fprintf(stderr, "no shell.qml at %s\n", qPrintable(QsPaths::shellPath()));
        return 1;
    }

    QQuickWindow::setDefaultAlphaBuffer(true); // transparent PanelWindows
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QsMacWindow::makeAccessoryApp();

    QLocalServer server;
    if (!Ipc::listen(&server)) {
        if (!replace) {
            std::fprintf(stderr, "already running for %s (use --replace)\n", qPrintable(QsPaths::shellPath()));
            return 1;
        }
        QJsonObject reply;
        Ipc::send({{"cmd", "kill"}}, &reply);
        QThread::msleep(300);
        if (!Ipc::listen(&server)) {
            std::fprintf(stderr, "could not take over the running instance\n");
            return 1;
        }
    }

    Shell shell(QsPaths::shellPath());
    if (!shell.load()) return 1;
    return app.exec();
}
