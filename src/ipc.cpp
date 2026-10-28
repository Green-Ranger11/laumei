#include "ipc.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <memory>
#include <utility>

#include "ipchandler.h"
#include "macnotify.h"
#include "notifications.h"
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

    if (cmd == "notify") {
        const QVariantMap fields = req.value("notification").toObject().toVariantMap();
        const int id = NotificationServer::post(fields);
        if (!id) MacNotify::post(fields); // no server in this shell: a macOS banner, as Laumei
        return {{"ok", true}, {"id", id}};
    }

    if (cmd == "kill") {
        QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
        return {{"ok", true}};
    }

    return {{"ok", false}, {"error", "unknown command " + cmd}};
}

// notify-send -w / -A: answers once the notification's action is invoked
// or it closes, rather than right away. False if `req` isn't such a request.
template <typename F> bool waitForNotification(const QJsonObject& req, QLocalSocket* sock, F respond) {
    if (req.value("cmd").toString() != "notify" || !req.value("wait").toBool()) return false;
    const QVariantMap fields = req.value("notification").toObject().toVariantMap();
    const int id = NotificationServer::post(fields);
    Notification* n = NotificationServer::find(id);
    if (!n) {
        // Not kept by the shell (or no server): nothing to wait for.
        if (!id) MacNotify::post(fields);
        respond({{"ok", true}, {"id", id}});
        return true;
    }
    auto done = std::make_shared<bool>(false);
    QObject::connect(n, &Notification::actionInvoked, sock, [=](const QString& action) {
        if (std::exchange(*done, true)) return;
        respond({{"ok", true}, {"id", id}, {"action", action}});
    });
    QObject::connect(n, &Notification::closed, sock, [=] {
        if (std::exchange(*done, true)) return;
        respond({{"ok", true}, {"id", id}});
    });
    return true;
}

} // namespace

namespace Ipc {

QString socketPath() {
    // $TMPDIR is per-user on macOS, so sockets don't collide between users.
    return QDir::tempPath() + "/laumei/" + QsPaths::shellId() + ".sock";
}

bool listen(QLocalServer* server) {
    QDir().mkpath(QDir::tempPath() + "/laumei");
    QLocalSocket probe;
    probe.connectToServer(socketPath());
    if (probe.waitForConnected(200)) return false;
    QLocalServer::removeServer(socketPath()); // stale socket from a crash

    QObject::connect(server, &QLocalServer::newConnection, server, [server] {
        while (QLocalSocket* sock = server->nextPendingConnection()) {
            QObject::connect(sock, &QLocalSocket::disconnected, sock, &QObject::deleteLater);
            QObject::connect(sock, &QLocalSocket::readyRead, sock, [sock] {
                if (!sock->canReadLine()) return;
                const QJsonObject req = QJsonDocument::fromJson(sock->readLine()).object();
                auto respond = [sock](const QJsonObject& reply) {
                    sock->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
                    sock->flush();
                    sock->disconnectFromServer();
                };
                if (!waitForNotification(req, sock, respond)) respond(handle(req));
            });
        }
    });
    return server->listen(socketPath());
}

bool send(const QJsonObject& request, QJsonObject* reply, int timeoutMs) {
    QLocalSocket sock;
    sock.connectToServer(socketPath());
    if (!sock.waitForConnected(1000)) return false;
    sock.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    sock.flush();
    while (!sock.canReadLine())
        if (!sock.waitForReadyRead(timeoutMs)) break;
    *reply = QJsonDocument::fromJson(sock.readLine()).object();
    return true;
}

} // namespace Ipc
