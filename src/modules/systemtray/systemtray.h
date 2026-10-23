#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

// Quickshell.Services.SystemTray, always empty on macOS: macOS 27 draws every
// menu-bar extra into one shared window, so there are no per-item icons or
// menus to mirror. Kept so tray widgets still load (and hide themselves).

class Status : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Status is an enum.")

public:
    enum Enum { Passive = 0, Active = 1, NeedsAttention = 2 };
    Q_ENUM(Enum)
};

class Category : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Category is an enum.")

public:
    enum Enum { ApplicationStatus = 0, SystemServices = 1, Hardware = 2 };
    Q_ENUM(Enum)
};

class SystemTrayItem : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SystemTrayItems come from SystemTray.items.")
    Q_PROPERTY(QString id MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QString title MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QString icon MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QString tooltipTitle MEMBER mEmpty CONSTANT)
    Q_PROPERTY(QString tooltipDescription MEMBER mEmpty CONSTANT)
    Q_PROPERTY(int status MEMBER mZero CONSTANT)
    Q_PROPERTY(int category MEMBER mZero CONSTANT)
    Q_PROPERTY(bool hasMenu MEMBER mFalse CONSTANT)
    Q_PROPERTY(bool onlyMenu MEMBER mFalse CONSTANT)
    Q_PROPERTY(QObject* menu MEMBER mMenu CONSTANT)

public:
    using QObject::QObject;
    Q_INVOKABLE void activate() {}
    Q_INVOKABLE void secondaryActivate() {}
    Q_INVOKABLE void scroll(int, bool) {}
    Q_INVOKABLE void display(QObject*, int, int) {}

private:
    QString mEmpty;
    int mZero = 0;
    bool mFalse = false;
    QObject* mMenu = nullptr;
};

class SystemTray : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(ObjectModel* items READ items CONSTANT)

public:
    explicit SystemTray(QObject* parent = nullptr) : QObject(parent), mItems(new ObjectModel(this)) {}
    ObjectModel* items() const { return mItems; }

private:
    ObjectModel* mItems;
};
