#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

// Quickshell.Services.Notifications, inert on macOS: apps post to Notification
// Center, which no other process can stand in for, so a NotificationServer
// here loads but never receives anything. Kept so configs that include one
// still load.

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

class NotificationAction : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("NotificationAction comes from a Notification.")
    Q_PROPERTY(QString identifier MEMBER mIdentifier CONSTANT)
    Q_PROPERTY(QString text MEMBER mText CONSTANT)

public:
    using QObject::QObject;
    Q_INVOKABLE void invoke() {}

private:
    QString mIdentifier, mText;
};

class Notification : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Notifications come from a NotificationServer.")
    Q_PROPERTY(int id MEMBER mId CONSTANT)
    Q_PROPERTY(bool tracked MEMBER mTracked NOTIFY trackedChanged)
    Q_PROPERTY(bool lastGeneration MEMBER mFalse CONSTANT)
    Q_PROPERTY(qreal expireTimeout MEMBER mZero CONSTANT)
    Q_PROPERTY(QString appName MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QString appIcon MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QString summary MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QString body MEMBER mEmpty CONSTANT)
    Q_PROPERTY(int urgency MEMBER mUrgency CONSTANT)
    Q_PROPERTY(QList<QObject*> actions MEMBER mActions CONSTANT)
    Q_PROPERTY(bool hasActionIcons MEMBER mFalse CONSTANT)
    Q_PROPERTY(bool resident MEMBER mFalse CONSTANT)
    Q_PROPERTY(bool transient MEMBER mFalse CONSTANT)
    Q_PROPERTY(QString desktopEntry MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QString image MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QVariantMap hints MEMBER mHints CONSTANT)

public:
    using QObject::QObject;
    Q_INVOKABLE void expire() {}
    Q_INVOKABLE void dismiss() {}

signals:
    void trackedChanged();
    void closed(int reason);

private:
    int mId = 0;
    bool mTracked = false;
    bool mFalse = false;
    qreal mZero = 0;
    QString mEmpty;
    int mUrgency = NotificationUrgency::Normal;
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
    ObjectModel* trackedNotifications() const { return mTracked; }

signals:
    void notification(Notification* notification);
    void settingsChanged();

private:
    ObjectModel* mTracked;
    bool mKeepOnReload = true, mPersistence = false, mBody = true, mBodyMarkup = false, mBodyLinks = false,
         mBodyImages = false, mActions = false, mActionIcons = false, mImage = false, mInlineReply = false;
    QStringList mExtraHints;
};
