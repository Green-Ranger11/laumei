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

class Notification;

// A notify-send -A action, reported back to a waiting notify-send; or the
// "default" action of a notification copied from Notification Center, which
// opens the app that posted it.
class NotificationAction : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("NotificationAction comes from a Notification.")
    Q_PROPERTY(QString identifier MEMBER mIdentifier CONSTANT)
    Q_PROPERTY(QString text MEMBER mText CONSTANT)

public:
    NotificationAction(const QString& identifier, const QString& text, const QString& bundleId, Notification* parent);
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
    Q_PROPERTY(qreal expireTimeout MEMBER mExpireTimeout NOTIFY changed)
    Q_PROPERTY(QString appName MEMBER mAppName NOTIFY changed)
    Q_PROPERTY(QString appIcon MEMBER mAppIcon NOTIFY changed)
    Q_PROPERTY(QString summary MEMBER mSummary NOTIFY changed)
    Q_PROPERTY(QString body MEMBER mBody NOTIFY changed)
    Q_PROPERTY(int urgency MEMBER mUrgency NOTIFY changed)
    Q_PROPERTY(QList<QObject*> actions MEMBER mActions NOTIFY changed)
    Q_PROPERTY(bool hasActionIcons MEMBER mFalse CONSTANT)
    Q_PROPERTY(bool resident MEMBER mFalse CONSTANT)
    Q_PROPERTY(bool transient MEMBER mTransient NOTIFY changed)
    Q_PROPERTY(QString desktopEntry MEMBER mDesktopEntry NOTIFY changed)
    Q_PROPERTY(QString image MEMBER mImage NOTIFY changed)
    Q_PROPERTY(QVariantMap hints MEMBER mHints NOTIFY changed)

public:
    // `fields` keys are the property names above, except actions, which is
    // a list of [identifier, text]; plus bundleId: the app the default
    // action opens.
    Notification(int id, const QVariantMap& fields, QObject* parent);
    // Replaces the content in place (notify-send -r).
    void update(const QVariantMap& fields);

    int id() const { return mId; }
    bool tracked() const { return mTracked; }
    // Untracking dismisses, as in Quickshell.
    void setTracked(bool tracked);
    Q_INVOKABLE void expire() { close(NotificationCloseReason::Expired); }
    Q_INVOKABLE void dismiss() { close(NotificationCloseReason::Dismissed); }

signals:
    void changed();
    void trackedChanged();
    void closed(int reason);
    // For a waiting notify-send.
    void actionInvoked(const QString& identifier);

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
    // newest server, or updates the live one `replacesId` names. Returns its
    // id, or 0 when the shell has no server.
    static int post(const QVariantMap& fields);
    // A tracked notification by id, or null.
    static Notification* find(int id);

signals:
    void notification(Notification* notification);
    void settingsChanged();

private:
    ObjectModel* mTracked;
    bool mKeepOnReload = true, mPersistence = false, mBody = true, mBodyMarkup = false, mBodyLinks = false,
         mBodyImages = false, mActions = false, mActionIcons = false, mImage = false, mInlineReply = false;
    QStringList mExtraHints;
};
