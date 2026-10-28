#include "mpris.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QUrl>
#include <QProcessEnvironment>

#include "paths.h"

#import <AppKit/AppKit.h>

namespace {

QProcessEnvironment childEnv() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", QsPaths::childPath());
    return env;
}

// "org.mozilla.firefox" -> "Firefox"
QString appName(const QString& bundleId) {
    if (bundleId.isEmpty()) return {};
    NSURL* url = [NSWorkspace.sharedWorkspace URLForApplicationWithBundleIdentifier:bundleId.toNSString()];
    if (!url) return bundleId.section('.', -1);
    QString name = QString::fromNSString([NSFileManager.defaultManager displayNameAtPath:url.path]);
    if (name.endsWith(".app")) name.chop(4);
    return name;
}

// Now-playing artwork (base64 from media-control) as a file URL, written to
// the cache once per image; the previous one is removed.
QString artUrl(const QJsonObject& s, const QString& previous) {
    const QByteArray data = QByteArray::fromBase64(s.value("artworkData").toString().toLatin1());
    if (data.isEmpty()) {
        if (!previous.isEmpty()) QFile::remove(QUrl(previous).toLocalFile());
        return {};
    }
    const QString ext = s.value("artworkMimeType").toString() == "image/png" ? ".png" : ".jpg";
    const QString path = QsPaths::cacheDir() + "/mpris-art-" +
                         QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex() + ext;
    const QString url = QUrl::fromLocalFile(path).toString();
    if (url == previous) return url;
    QDir().mkpath(QsPaths::cacheDir());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) return {};
    if (!previous.isEmpty()) QFile::remove(QUrl(previous).toLocalFile());
    return url;
}

} // namespace

qreal MprisPlayer::position() const {
    if (!mPlaying || !mStamp.isValid()) return mElapsed;
    return mElapsed + mStamp.msecsTo(QDateTime::currentDateTimeUtc()) / 1000.0 * (mRate > 0 ? mRate : 1);
}

void MprisPlayer::apply(const QJsonObject& s) {
    mBundleId = s.value("bundleIdentifier").toString();
    mIdentity = appName(mBundleId);
    mTitle = s.value("title").toString();
    mArtist = s.value("artist").toString();
    mAlbum = s.value("album").toString();
    mArtUrl = artUrl(s, mArtUrl);
    mPlaying = s.value("playing").toBool();
    mRate = s.value("playbackRate").toDouble();
    mElapsed = s.value("elapsedTime").toDouble();
    // Live streams report an absurd duration.
    const double d = s.value("duration").toDouble();
    mLength = d > 0 && d < 1e9 ? d : 0;
    mStamp = QDateTime::fromString(s.value("timestamp").toString(), Qt::ISODate);
    emit changed();
    emit positionChanged();
}

void MprisPlayer::command(const QStringList& args) {
    QProcess p;
    p.setProcessEnvironment(childEnv());
    p.setProgram("/bin/sh");
    p.setArguments(QStringList{"-c", "exec media-control \"$@\"", "media-control"} + args);
    p.startDetached();
}

Mpris::Mpris(QObject* parent) : QObject(parent), mPlayers(new ObjectModel(this)), mPlayer(new MprisPlayer(this)) {
    mStream.setProcessEnvironment(childEnv());
    connect(&mStream, &QProcess::readyReadStandardOutput, this, [this] {
        mBuffer += mStream.readAllStandardOutput();
        qsizetype nl;
        while ((nl = mBuffer.indexOf('\n')) != -1) {
            onLine(mBuffer.left(nl));
            mBuffer.remove(0, nl + 1);
        }
    });
    // The stream dies with media-control updates or when it isn't installed
    // yet; retry quietly.
    mRestart.setSingleShot(true);
    mRestart.setInterval(5000);
    connect(&mRestart, &QTimer::timeout, this, &Mpris::start);
    connect(&mStream, &QProcess::finished, &mRestart, qOverload<>(&QTimer::start));
    connect(&mStream, &QProcess::errorOccurred, &mRestart, qOverload<>(&QTimer::start));

    // Position moves without events while playing.
    mTick.setInterval(1000);
    connect(&mTick, &QTimer::timeout, mPlayer, &MprisPlayer::positionChanged);
    mTick.start();
    start();
}

Mpris::~Mpris() {
    mStream.disconnect(this);
    mStream.kill();
    mStream.waitForFinished(500);
}

void Mpris::start() {
    if (mStream.state() != QProcess::NotRunning) return;
    mBuffer.clear();
    // Diffs, so the artwork (hundreds of KB) only comes when it changes.
    mStream.start("/bin/sh", {"-c", "exec media-control stream"});
}

void Mpris::onLine(const QByteArray& line) {
    const QJsonObject msg = QJsonDocument::fromJson(line).object();
    if (msg.value("type").toString() != "data") return;
    const QJsonObject payload = msg.value("payload").toObject();
    if (msg.value("diff").toBool()) {
        for (auto it = payload.begin(); it != payload.end(); ++it) mState[it.key()] = it.value();
    } else {
        mState = payload;
    }

    const bool active = !mState.value("bundleIdentifier").toString().isEmpty();
    if (active) mPlayer->apply(mState);
    if (active && !mPlayers->contains(mPlayer)) mPlayers->append(mPlayer);
    if (!active && mPlayers->contains(mPlayer)) mPlayers->remove(mPlayer);
}
