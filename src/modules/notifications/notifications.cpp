#include "notifications.h"

#include <QDebug>

NotificationServer::NotificationServer(QObject* parent) : QObject(parent), mTracked(new ObjectModel(this)) {
    qInfo("NotificationServer: inert on macOS (apps post to Notification Center); it will stay empty.");
}
