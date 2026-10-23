#include "fileview.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>

// Error codes match Quickshell's FileViewError enum.
enum { ErrUnknown = 0, ErrNotFound = 1, ErrPermissionDenied = 2, ErrNotAFile = 3 };

FileView::FileView(QObject* parent) : QObject(parent) {
    connect(&mWatcher, &QFileSystemWatcher::fileChanged, this, [this] {
        updateWatch(); // editors replace the file, which drops the watch
        emit fileChanged();
    });
}

void FileView::componentComplete() {
    mComplete = true;
    updateWatch();
    if (mPreload && !mPath.isEmpty()) reload();
}

QString FileView::localPath() const {
    if (mPath.startsWith("file:")) return QUrl(mPath).toLocalFile();
    return mPath;
}

void FileView::setPath(const QString& p) {
    if (p == mPath) return;
    mPath = p;
    mData.clear();
    mHaveData = false;
    emit pathChanged();
    if (!mComplete) return;
    updateWatch();
    if (mPreload && !mPath.isEmpty()) reload();
}

void FileView::setWatchChanges(bool w) {
    if (w == mWatchChanges) return;
    mWatchChanges = w;
    emit watchChangesChanged();
    updateWatch();
}

void FileView::updateWatch() {
    if (!mWatcher.files().isEmpty()) mWatcher.removePaths(mWatcher.files());
    const QString p = localPath();
    if (mWatchChanges && mComplete && !p.isEmpty() && QFileInfo::exists(p)) mWatcher.addPath(p);
}

bool FileView::readNow() {
    const QString p = localPath();
    QFile f(p);
    int err = ErrUnknown;
    if (!QFileInfo::exists(p)) err = ErrNotFound;
    else if (QFileInfo(p).isDir()) err = ErrNotAFile;
    else if (f.open(QIODevice::ReadOnly)) {
        mData = f.readAll();
        mHaveData = true;
        return true;
    } else if (f.error() == QFileDevice::PermissionsError) err = ErrPermissionDenied;

    if (mPrintErrors) qWarning().noquote() << "FileView: failed to read" << p;
    // Queued like the success path, so handlers see a fully built tree.
    QMetaObject::invokeMethod(this, [this, err] { emit loadFailed(err); }, Qt::QueuedConnection);
    return false;
}

void FileView::reload() {
    if (mPath.isEmpty()) return;
    if (readNow()) QMetaObject::invokeMethod(this, [this] { emit loaded(); }, Qt::QueuedConnection);
}

QString FileView::text() { return QString::fromUtf8(data()); }

QByteArray FileView::data() {
    if (!mHaveData && !mPath.isEmpty()) readNow();
    return mData;
}

void FileView::setText(const QString& text) { setData(text.toUtf8()); }

void FileView::setData(const QByteArray& data) {
    const QString p = localPath();
    QDir().mkpath(QFileInfo(p).absolutePath());

    bool ok;
    if (mAtomicWrites) {
        QSaveFile f(p);
        ok = f.open(QIODevice::WriteOnly) && f.write(data) == data.size() && f.commit();
    } else {
        QFile f(p);
        ok = f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(data) == data.size();
    }

    if (!ok) {
        if (mPrintErrors) qWarning().noquote() << "FileView: failed to write" << p;
        emit saveFailed(ErrUnknown);
        return;
    }
    mData = data;
    mHaveData = true;
    updateWatch();
    emit saved();
}
