#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

// Quickshell.Services.Notifications on macOS. Apps post to Notification
// Center, which no other process can stand in for, so a NotificationServer
// gets two feeds instead: `notify-send` run against this shell, and copies of
// what lands in Notification Center (read from its database, see ncbridge.mm).
// Actions, replacement and images aren't supported.

class NotificationUrgency : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("NotificationUrgency is an enum.")

public:
    enum Enum { Low = 0, Normal = 1, Critical = 2 };
    Q_ENUM(Enum)
};

class NotificationCloseReason : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("NotificationCloseReason is an enum.")

public:
    enum Enum { Expired = 1, Dismissed = 2, CloseRequested = 3 };
    Q_ENUM(Enum)
};

// The only action Laumei makes: "default" on a notification copied from
// Notification Center, opening the app that posted it.
class NotificationAction : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("NotificationAction comes from a Notification.")
    Q_PROPERTY(QString identifier MEMBER mIdentifier CONSTANT)
    Q_PROPERTY(QString text MEMBER mText CONSTANT)

public:
    NotificationAction(const QString& bundleId, QObject* parent)
        : QObject(parent), mIdentifier("default"), mText("Open"), mBundleId(bundleId) {}
    Q_INVOKABLE void invoke();

private:
    QString mIdentifier, mText, mBundleId;
};

class Notification : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Notifications come from a NotificationServer.")
    Q_PROPERTY(int id MEMBER mId CONSTANT)
    Q_PROPERTY(bool tracked READ tracked WRITE setTracked NOTIFY trackedChanged)
    Q_PROPERTY(bool lastGeneration MEMBER mFalse CONSTANT)
    Q_PROPERTY(qreal expireTimeout MEMBER mExpireTimeout CONSTANT)
    Q_PROPERTY(QString appName MEMBER mAppName CONSTANT)
    Q_PROPERTY(QString appIcon MEMBER mAppIcon CONSTANT)
    Q_PROPERTY(QString summary MEMBER mSummary CONSTANT)
    Q_PROPERTY(QString body MEMBER mBody CONSTANT)
    Q_PROPERTY(int urgency MEMBER mUrgency CONSTANT)
    Q_PROPERTY(QList<QObject*> actions MEMBER mActions CONSTANT)
    Q_PROPERTY(bool hasActionIcons MEMBER mFalse CONSTANT)
    Q_PROPERTY(bool resident MEMBER mFalse CONSTANT)
    Q_PROPERTY(bool transient MEMBER mTransient CONSTANT)
    Q_PROPERTY(QString desktopEntry MEMBER mDesktopEntry CONSTANT)
    Q_PROPERTY(QString image MEMBER mImage CONSTANT)
    Q_PROPERTY(QVariantMap hints MEMBER mHints CONSTANT)

public:
    // `fields` keys are the property names above, plus bundleId: the app
    // to open from the default action.
    Notification(int id, const QVariantMap& fields, QObject* parent);

    int id() const { return mId; }
    bool tracked() const { return mTracked; }
    // Untracking dismisses, as in Quickshell.
    void setTracked(bool tracked);
    Q_INVOKABLE void expire() { close(NotificationCloseReason::Expired); }
    Q_INVOKABLE void dismiss() { close(NotificationCloseReason::Dismissed); }

signals:
    void trackedChanged();
    void closed(int reason);

private:
    void close(int reason);

    int mId;
    bool mTracked = false;
    bool mClosed = false;
    bool mFalse = false;
    bool mTransient;
    qreal mExpireTimeout;
    QString mAppName, mAppIcon, mSummary, mBody, mDesktopEntry, mImage;
    int mUrgency;
    QList<QObject*> mActions;
    QVariantMap mHints;
};

class NotificationServer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(ObjectModel* trackedNotifications READ trackedNotifications CONSTANT)
    Q_PROPERTY(bool keepOnReload MEMBER mKeepOnReload NOTIFY settingsChanged)
    Q_PROPERTY(bool persistenceSupported MEMBER mPersistence NOTIFY settingsChanged)
    Q_PROPERTY(bool bodySupported MEMBER mBody NOTIFY settingsChanged)
    Q_PROPERTY(bool bodyMarkupSupported MEMBER mBodyMarkup NOTIFY settingsChanged)
    Q_PROPERTY(bool bodyHyperlinksSupported MEMBER mBodyLinks NOTIFY settingsChanged)
    Q_PROPERTY(bool bodyImagesSupported MEMBER mBodyImages NOTIFY settingsChanged)
    Q_PROPERTY(bool actionsSupported MEMBER mActions NOTIFY settingsChanged)
    Q_PROPERTY(bool actionIconsSupported MEMBER mActionIcons NOTIFY settingsChanged)
    Q_PROPERTY(bool imageSupported MEMBER mImage NOTIFY settingsChanged)
    Q_PROPERTY(bool inlineReplySupported MEMBER mInlineReply NOTIFY settingsChanged)
    Q_PROPERTY(QStringList extraHints MEMBER mExtraHints NOTIFY settingsChanged)

public:
    explicit NotificationServer(QObject* parent = nullptr);
    ~NotificationServer() override;
    ObjectModel* trackedNotifications() const { return mTracked; }

    // Hands a notification (Notification's property names as keys) to the
    // newest server. Returns its id, or 0 when the shell has no server.
    static int post(const QVariantMap& fields);

signals:
    void notification(Notification* notification);
    void settingsChanged();

private:
    ObjectModel* mTracked;
    bool mKeepOnReload = true, mPersistence = false, mBody = true, mBodyMarkup = false, mBodyLinks = false,
         mBodyImages = false, mActions = false, mActionIcons = false, mImage = false, mInlineReply = false;
    QStringList mExtraHints;
};
