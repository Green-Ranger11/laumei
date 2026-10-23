#pragma once

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QObject>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqmlregistration.h>

// Quickshell.Io FileView: reads (and writes) one file. Loads on creation and
// whenever `path` changes; loaded() fires after each successful read.
class FileView : public QObject, public QQmlParserStatus {
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool preload MEMBER mPreload NOTIFY preloadChanged)
    Q_PROPERTY(bool blockLoading MEMBER mBlockLoading NOTIFY blockLoadingChanged)
    Q_PROPERTY(bool blockAllReads MEMBER mBlockAllReads NOTIFY blockAllReadsChanged)
    Q_PROPERTY(bool printErrors MEMBER mPrintErrors NOTIFY printErrorsChanged)
    Q_PROPERTY(bool atomicWrites MEMBER mAtomicWrites NOTIFY atomicWritesChanged)
    Q_PROPERTY(bool watchChanges READ watchChanges WRITE setWatchChanges NOTIFY watchChangesChanged)

public:
    explicit FileView(QObject* parent = nullptr);

    void classBegin() override {}
    void componentComplete() override;

    QString path() const { return mPath; }
    void setPath(const QString& p);
    bool watchChanges() const { return mWatchChanges; }
    void setWatchChanges(bool w);

    Q_INVOKABLE QString text();
    Q_INVOKABLE QByteArray data();
    Q_INVOKABLE void setText(const QString& text);
    Q_INVOKABLE void setData(const QByteArray& data);
    Q_INVOKABLE void reload();
    Q_INVOKABLE bool waitForJob() { return true; } // reads and writes are synchronous

signals:
    void pathChanged();
    void preloadChanged();
    void blockLoadingChanged();
    void blockAllReadsChanged();
    void printErrorsChanged();
    void atomicWritesChanged();
    void watchChangesChanged();
    void loaded();
    void loadFailed(int error);
    void saved();
    void saveFailed(int error);
    void fileChanged();

private:
    QString localPath() const;
    bool readNow();
    void updateWatch();

    QString mPath;
    QByteArray mData;
    bool mHaveData = false;
    bool mComplete = false;
    bool mPreload = true;
    bool mBlockLoading = false;
    bool mBlockAllReads = false;
    bool mPrintErrors = true;
    bool mAtomicWrites = true;
    bool mWatchChanges = false;
    QFileSystemWatcher mWatcher;
};
