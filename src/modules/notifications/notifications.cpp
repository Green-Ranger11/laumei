#include "notifications.h"

#include <QProcess>

#include "ncbridge.h"

namespace {
QList<NotificationServer*> g_servers;
int g_nextId = 0;
} // namespace

Notification::Notification(int id, const QVariantMap& f, QObject* parent)
    : QObject(parent), mId(id), mTransient(f.value("transient").toBool()),
      mExpireTimeout(f.value("expireTimeout", -1).toReal()), mAppName(f.value("appName").toString()),
      mAppIcon(f.value("appIcon").toString()), mSummary(f.value("summary").toString()),
      mBody(f.value("body").toString()), mDesktopEntry(f.value("desktopEntry").toString()),
      mImage(f.value("image").toString()), mUrgency(f.value("urgency", NotificationUrgency::Normal).toInt()),
      mHints(f.value("hints").toMap()) {
    if (const QString bundleId = f.value("bundleId").toString(); !bundleId.isEmpty())
        mActions << new NotificationAction(bundleId, this);
}

void NotificationAction::invoke() { QProcess::startDetached("/usr/bin/open", {"-b", mBundleId}); }

void Notification::setTracked(bool tracked) {
    if (tracked == mTracked) return;
    if (!tracked) return dismiss();
    mTracked = true;
    emit trackedChanged();
}

void Notification::close(int reason) {
    if (mClosed) return;
    mClosed = true;
    emit closed(reason);
    if (mTracked) {
        mTracked = false;
        emit trackedChanged();
    }
    deleteLater();
}

NotificationServer::NotificationServer(QObject* parent) : QObject(parent), mTracked(new ObjectModel(this)) {
    g_servers.append(this);
    NcBridge::start();
}

NotificationServer::~NotificationServer() { g_servers.removeAll(this); }

int NotificationServer::post(const QVariantMap& fields) {
    if (g_servers.isEmpty()) return 0;
    NotificationServer* server = g_servers.last();
    auto* n = new Notification(++g_nextId, fields, server);
    connect(n, &Notification::trackedChanged, server, [server, n] {
        if (n->tracked()) server->mTracked->append(n);
        else server->mTracked->remove(n);
    });
    connect(n, &QObject::destroyed, server, [server, n] { server->mTracked->remove(n); });
    emit server->notification(n);
    // Nobody kept it: discarded, as in Quickshell.
    if (!n->tracked()) n->deleteLater();
    return n->id();
}
