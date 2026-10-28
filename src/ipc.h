#pragma once

#include <QJsonObject>
#include <QLocalServer>
#include <QString>

// Local-socket IPC between a running shell and `laumei ipc ...` / `laumei kill`.
// One JSON request per connection, one JSON reply.
namespace Ipc {

// Socket for the shell identified by QsPaths::shellId().
QString socketPath();

// Starts listening. Returns false if another live instance owns the socket.
bool listen(QLocalServer* server);

// Sends `request` to the running instance; `reply` gets its answer, waited
// for up to `timeoutMs` (-1: forever). Returns false if no instance is running.
bool send(const QJsonObject& request, QJsonObject* reply, int timeoutMs = 5000);

} // namespace Ipc
