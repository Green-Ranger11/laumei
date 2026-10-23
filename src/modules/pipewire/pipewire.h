#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "objectmodel.h"

// Quickshell.Services.Pipewire on macOS, backed by CoreAudio: each audio
// device is a node; the system default output/input are the default sink
// and source.

class PwNode;

class PwNodeAudio : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("PwNodeAudio comes from PwNode.audio.")
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumesChanged)
    Q_PROPERTY(QList<qreal> volumes READ volumes NOTIFY volumesChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)

public:
    explicit PwNodeAudio(PwNode* node);

    qreal volume() const { return mVolume; }
    void setVolume(qreal v);
    QList<qreal> volumes() const { return {mVolume}; }
    bool muted() const { return mMuted; }
    void setMuted(bool m);

    void refresh();

signals:
    void volumesChanged();
    void mutedChanged();

private:
    PwNode* mNode;
    qreal mVolume = 0;
    bool mMuted = false;
};

class PwNode : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("PwNode comes from Pipewire.")
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString nickname READ description CONSTANT)
    Q_PROPERTY(bool isSink READ isSink CONSTANT)
    Q_PROPERTY(bool isStream READ isStream CONSTANT)
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(PwNodeAudio* audio READ audio CONSTANT)
    Q_PROPERTY(QVariantMap properties READ properties CONSTANT)

public:
    PwNode(unsigned deviceId, bool sink, QObject* parent);
    ~PwNode() override;

    int id() const { return int(mDevice); }
    QString name() const { return mName; }
    QString description() const { return mDescription; }
    bool isSink() const { return mSink; }
    bool isStream() const { return false; }
    bool ready() const { return true; }
    PwNodeAudio* audio() const { return mAudio; }
    QVariantMap properties() const;

    unsigned device() const { return mDevice; }

private:
    unsigned mDevice;
    bool mSink;
    QString mName, mDescription;
    PwNodeAudio* mAudio;
    void* mListener = nullptr;
};

// Keeps nodes bound on Linux; CoreAudio nodes are always live.
class PwObjectTracker : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QList<QObject*> objects MEMBER mObjects NOTIFY objectsChanged)

public:
    using QObject::QObject;

signals:
    void objectsChanged();

private:
    QList<QObject*> mObjects;
};

class Pipewire : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(ObjectModel* nodes READ nodes CONSTANT)
    Q_PROPERTY(PwNode* defaultAudioSink READ defaultAudioSink NOTIFY defaultAudioSinkChanged)
    Q_PROPERTY(PwNode* defaultAudioSource READ defaultAudioSource NOTIFY defaultAudioSourceChanged)
    Q_PROPERTY(PwNode* preferredDefaultAudioSink READ defaultAudioSink WRITE setDefaultAudioSink NOTIFY defaultAudioSinkChanged)
    Q_PROPERTY(PwNode* preferredDefaultAudioSource READ defaultAudioSource WRITE setDefaultAudioSource NOTIFY defaultAudioSourceChanged)
    Q_PROPERTY(bool ready READ ready CONSTANT)

public:
    explicit Pipewire(QObject* parent = nullptr);
    ~Pipewire() override;

    ObjectModel* nodes() const { return mNodes; }
    PwNode* defaultAudioSink() const { return mSink; }
    PwNode* defaultAudioSource() const { return mSource; }
    void setDefaultAudioSink(PwNode* n);
    void setDefaultAudioSource(PwNode* n);
    bool ready() const { return true; }

    void rescan();
    void updateDefaults();

signals:
    void defaultAudioSinkChanged();
    void defaultAudioSourceChanged();

private:
    PwNode* find(unsigned device, bool sink) const;

    ObjectModel* mNodes;
    PwNode* mSink = nullptr;
    PwNode* mSource = nullptr;
    void* mListener = nullptr;
};
