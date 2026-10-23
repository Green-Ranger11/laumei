#include "ipc.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>

#include "ipchandler.h"
#include "paths.h"

namespace {

QJsonObject handle(const QJsonObject& req) {
    const QString cmd = req.value("cmd").toString();

    if (cmd == "show") {
        QJsonArray targets;
        for (IpcHandler* h : IpcHandler::handlers())
            targets.append(QJsonObject{{"target", h->target()},
                                       {"functions", QJsonArray::fromStringList(h->describeFunctions())}});
        return {{"ok", true}, {"targets", targets}};
    }

    if (cmd == "call") {
        const QString target = req.value("target").toString();
        IpcHandler* h = IpcHandler::find(target);
        if (!h) return {{"ok", false}, {"error", "no target " + target}};
        QStringList args;
        for (const auto& a : req.value("args").toArray()) args << a.toString();
        QString result, error;
        if (!h->call(req.value("function").toString(), args, &result, &error))
            return {{"ok", false}, {"error", error}};
        return {{"ok", true}, {"result", result}};
    }

    if (cmd == "kill") {
        QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
        return {{"ok", true}};
    }

    return {{"ok", false}, {"error", "unknown command " + cmd}};
}

} // namespace

namespace Ipc {

QString socketPath() {
    // $TMPDIR is per-user on macOS, so sockets don't collide between users.
    return QDir::tempPath() + "/quickshell-macos/" + QsPaths::shellId() + ".sock";
}

bool listen(QLocalServer* server) {
    QDir().mkpath(QDir::tempPath() + "/quickshell-macos");
    QLocalSocket probe;
    probe.connectToServer(socketPath());
    if (probe.waitForConnected(200)) return false;
    QLocalServer::removeServer(socketPath()); // stale socket from a crash

    QObject::connect(server, &QLocalServer::newConnection, server, [server] {
        while (QLocalSocket* sock = server->nextPendingConnection()) {
            QObject::connect(sock, &QLocalSocket::disconnected, sock, &QObject::deleteLater);
            QObject::connect(sock, &QLocalSocket::readyRead, sock, [sock] {
                if (!sock->canReadLine()) return;
                const QJsonObject reply = handle(QJsonDocument::fromJson(sock->readLine()).object());
                sock->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
                sock->flush();
                sock->disconnectFromServer();
            });
        }
    });
    return server->listen(socketPath());
}

bool send(const QJsonObject& request, QJsonObject* reply) {
    QLocalSocket sock;
    sock.connectToServer(socketPath());
    if (!sock.waitForConnected(1000)) return false;
    sock.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    sock.flush();
    while (!sock.canReadLine())
        if (!sock.waitForReadyRead(5000)) break;
    *reply = QJsonDocument::fromJson(sock.readLine()).object();
    return true;
}

} // namespace Ipc
