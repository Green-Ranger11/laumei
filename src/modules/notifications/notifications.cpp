#include "notifications.h"

#include <QProcess>

#include "ncbridge.h"

namespace {
QList<NotificationServer*> g_servers;
int g_nextId = 0;
} // namespace

Notification::Notification(int id, const QVariantMap& fields, QObject* parent) : QObject(parent), mId(id) {
    update(fields);
}

void Notification::update(const QVariantMap& f) {
    mTransient = f.value("transient").toBool();
    mExpireTimeout = f.value("expireTimeout", -1).toReal();
    mAppName = f.value("appName").toString();
    mAppIcon = f.value("appIcon").toString();
    mSummary = f.value("summary").toString();
    mBody = f.value("body").toString();
    mDesktopEntry = f.value("desktopEntry").toString();
    mImage = f.value("image").toString();
    mUrgency = f.value("urgency", NotificationUrgency::Normal).toInt();
    mHints = f.value("hints").toMap();

    qDeleteAll(mActions);
    mActions.clear();
    for (const QVariant& a : f.value("actions").toList()) {
        const QVariantList pair = a.toList();
        mActions << new NotificationAction(pair.value(0).toString(), pair.value(1).toString(), {}, this);
    }
    if (const QString bundleId = f.value("bundleId").toString(); !bundleId.isEmpty())
        mActions << new NotificationAction("default", "Open", bundleId, this);
    emit changed();
}

NotificationAction::NotificationAction(const QString& identifier, const QString& text, const QString& bundleId,
                                       Notification* parent)
    : QObject(parent), mIdentifier(identifier), mText(text), mBundleId(bundleId) {}

void NotificationAction::invoke() {
    if (!mBundleId.isEmpty()) QProcess::startDetached("/usr/bin/open", {"-b", mBundleId});
    emit static_cast<Notification*>(parent())->actionInvoked(mIdentifier);
}

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

Notification* NotificationServer::find(int id) {
    for (NotificationServer* server : g_servers)
        for (auto* n : server->findChildren<Notification*>(Qt::FindDirectChildrenOnly))
            if (n->id() == id && n->tracked()) return n;
    return nullptr;
}

int NotificationServer::post(const QVariantMap& fields) {
    if (g_servers.isEmpty()) return 0;
    if (Notification* old = find(fields.value("replacesId").toInt())) {
        old->update(fields);
        return old->id();
    }
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
