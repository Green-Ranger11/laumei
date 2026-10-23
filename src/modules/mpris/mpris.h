#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

// Quickshell.Services.Mpris on macOS. macOS has one system-wide "Now
// Playing" session rather than a player per app; it is read through the
// media-control tool (brew install media-control), since Apple restricts the
// MediaRemote framework to its own apps. At most one player is listed.

class MprisPlaybackState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("MprisPlaybackState is an enum.")

public:
    enum Enum { Stopped = 0, Playing = 1, Paused = 2 };
    Q_ENUM(Enum)
};

class MprisPlayer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("MprisPlayer comes from Mpris.players.")
    Q_PROPERTY(QString identity READ identity NOTIFY changed)
    Q_PROPERTY(QString desktopEntry READ desktopEntry NOTIFY changed)
    Q_PROPERTY(QString dbusName READ desktopEntry NOTIFY changed)
    Q_PROPERTY(QString uniqueId READ desktopEntry NOTIFY changed)
    Q_PROPERTY(bool isPlaying READ isPlaying WRITE setPlaying NOTIFY changed)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY changed)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY changed)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY changed)
    Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY changed)
    Q_PROPERTY(QString trackArtUrl READ trackArtUrl NOTIFY changed)
    Q_PROPERTY(qreal length READ length NOTIFY changed)
    Q_PROPERTY(bool lengthSupported READ lengthSupported NOTIFY changed)
    Q_PROPERTY(qreal position READ position NOTIFY positionChanged)
    Q_PROPERTY(bool positionSupported READ lengthSupported NOTIFY changed)
    Q_PROPERTY(bool canControl READ yes CONSTANT)
    Q_PROPERTY(bool canPlay READ yes CONSTANT)
    Q_PROPERTY(bool canPause READ yes CONSTANT)
    Q_PROPERTY(bool canTogglePlaying READ yes CONSTANT)
    Q_PROPERTY(bool canGoNext READ yes CONSTANT)
    Q_PROPERTY(bool canGoPrevious READ yes CONSTANT)
    Q_PROPERTY(bool canSeek READ lengthSupported NOTIFY changed)

public:
    using QObject::QObject;

    QString identity() const { return mIdentity; }
    QString desktopEntry() const { return mBundleId; }
    bool isPlaying() const { return mPlaying; }
    void setPlaying(bool p) { p ? play() : pause(); }
    int playbackState() const { return mPlaying ? MprisPlaybackState::Playing : MprisPlaybackState::Paused; }
    QString trackTitle() const { return mTitle; }
    QString trackArtist() const { return mArtist; }
    QString trackAlbum() const { return mAlbum; }
    QString trackArtUrl() const { return {}; }
    qreal length() const { return mLength; }
    bool lengthSupported() const { return mLength > 0; }
    qreal position() const;
    bool yes() const { return true; }

    Q_INVOKABLE void togglePlaying() { command({"toggle-play-pause"}); }
    Q_INVOKABLE void play() { command({"play"}); }
    Q_INVOKABLE void pause() { command({"pause"}); }
    Q_INVOKABLE void stop() { command({"stop"}); }
    Q_INVOKABLE void next() { command({"next-track"}); }
    Q_INVOKABLE void previous() { command({"previous-track"}); }
    Q_INVOKABLE void seek(qreal offset) { command({"seek", QString::number(position() + offset)}); }

    void apply(const QJsonObject& state);

signals:
    void changed();
    void positionChanged();

private:
    static void command(const QStringList& args);

    QString mIdentity, mBundleId, mTitle, mArtist, mAlbum;
    bool mPlaying = false;
    qreal mLength = 0;
    qreal mElapsed = 0;
    qreal mRate = 0;
    QDateTime mStamp;
};

class Mpris : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(ObjectModel* players READ players CONSTANT)

public:
    explicit Mpris(QObject* parent = nullptr);
    ~Mpris() override;

    ObjectModel* players() const { return mPlayers; }

private:
    void start();
    void onLine(const QByteArray& line);

    ObjectModel* mPlayers;
    MprisPlayer* mPlayer;
    QProcess mStream;
    QByteArray mBuffer;
    QJsonObject mState;
    QTimer mRestart;
    QTimer mTick;
};
