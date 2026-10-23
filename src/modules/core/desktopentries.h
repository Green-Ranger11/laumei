#pragma once

#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// One application. On macOS each .app bundle becomes an entry; fields follow
// Quickshell's DesktopEntry (freedesktop .desktop) shape.
class DesktopEntry : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DesktopEntry comes from DesktopEntries.")
    Q_PROPERTY(QString id MEMBER mId CONSTANT)
    Q_PROPERTY(QString name MEMBER mName CONSTANT)
    Q_PROPERTY(QString genericName MEMBER mGenericName CONSTANT)
    Q_PROPERTY(QString comment MEMBER mComment CONSTANT)
    Q_PROPERTY(QString icon MEMBER mIcon CONSTANT)
    Q_PROPERTY(bool noDisplay MEMBER mNoDisplay CONSTANT)
    Q_PROPERTY(QStringList categories MEMBER mCategories CONSTANT)
    Q_PROPERTY(QStringList keywords MEMBER mKeywords CONSTANT)
    Q_PROPERTY(QString execString READ execString CONSTANT)
    Q_PROPERTY(QStringList command READ command CONSTANT)
    Q_PROPERTY(QString workingDirectory MEMBER mWorkingDirectory CONSTANT)
    Q_PROPERTY(bool runInTerminal MEMBER mRunInTerminal CONSTANT)
    Q_PROPERTY(QList<QObject*> actions READ actions CONSTANT)

public:
    using QObject::QObject;

    QString execString() const;
    QStringList command() const;
    QList<QObject*> actions() const { return {}; }
    Q_INVOKABLE void execute() const;

    QString mId, mName, mGenericName, mComment, mIcon, mWorkingDirectory;
    QString mBundlePath;
    QStringList mCategories, mKeywords;
    bool mNoDisplay = false;
    bool mRunInTerminal = false;
};

// `DesktopEntries.applications.values`, mirroring Quickshell's ObjectModel.
class DesktopEntryModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use DesktopEntries.applications.")
    Q_PROPERTY(QList<QObject*> values READ values NOTIFY valuesChanged)

public:
    using QObject::QObject;
    QList<QObject*> values() const { return mValues; }
    void setValues(const QList<QObject*>& v);

signals:
    void valuesChanged();

private:
    QList<QObject*> mValues;
};

// Installed applications, rescanned when the Applications folders change.
class DesktopEntries : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(DesktopEntryModel* applications READ applications CONSTANT)

public:
    explicit DesktopEntries(QObject* parent = nullptr);

    DesktopEntryModel* applications() const { return mApplications; }
    Q_INVOKABLE DesktopEntry* byId(const QString& id) const;
    // Best-effort match on id, name or bundle file name, case-insensitive.
    Q_INVOKABLE DesktopEntry* heuristicLookup(const QString& name) const;

private:
    void scan();

    DesktopEntryModel* mApplications;
    QFileSystemWatcher mWatcher;
    QTimer mRescan;
};
