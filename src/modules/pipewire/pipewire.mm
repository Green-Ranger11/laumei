#include "pipewire.h"

#include <vector>

#import <AudioToolbox/AudioServices.h>
#import <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>

namespace {

AudioObjectPropertyAddress addr(AudioObjectPropertySelector sel,
                                AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
                                AudioObjectPropertyElement el = kAudioObjectPropertyElementMain) {
    return {sel, scope, el};
}

AudioObjectPropertyScope scopeFor(bool sink) {
    return sink ? kAudioDevicePropertyScopeOutput : kAudioDevicePropertyScopeInput;
}

QString stringProp(AudioObjectID obj, AudioObjectPropertySelector sel) {
    CFStringRef s = nullptr;
    UInt32 size = sizeof(s);
    auto a = addr(sel);
    if (AudioObjectGetPropertyData(obj, &a, 0, nullptr, &size, &s) != noErr || !s) return {};
    const QString out = QString::fromCFString(s);
    CFRelease(s);
    return out;
}

bool hasStreams(AudioObjectID dev, bool sink) {
    auto a = addr(kAudioDevicePropertyStreams, scopeFor(sink));
    UInt32 size = 0;
    return AudioObjectGetPropertyDataSize(dev, &a, 0, nullptr, &size) == noErr && size > 0;
}

AudioObjectID defaultDevice(bool sink) {
    AudioObjectID dev = kAudioObjectUnknown;
    UInt32 size = sizeof(dev);
    auto a = addr(sink ? kAudioHardwarePropertyDefaultOutputDevice : kAudioHardwarePropertyDefaultInputDevice);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, &dev);
    return dev;
}

// The volume macOS's own slider moves: the virtual main volume when the
// device has one, else the main element's scalar.
AudioObjectPropertyAddress volumeAddr(AudioObjectID dev, bool sink) {
    auto a = addr(kAudioHardwareServiceDeviceProperty_VirtualMainVolume, scopeFor(sink));
    if (AudioObjectHasProperty(dev, &a)) return a;
    return addr(kAudioDevicePropertyVolumeScalar, scopeFor(sink));
}

// Listener blocks registered for one object, removed together.
struct Listeners {
    AudioObjectID object;
    std::vector<std::pair<AudioObjectPropertyAddress, AudioObjectPropertyListenerBlock>> blocks;

    void add(AudioObjectPropertyAddress a, void (^fn)(void)) {
        AudioObjectPropertyListenerBlock b = [^(UInt32, const AudioObjectPropertyAddress*) { fn(); } copy];
        if (AudioObjectAddPropertyListenerBlock(object, &a, dispatch_get_main_queue(), b) == noErr)
            blocks.emplace_back(a, b);
        else
            [b release];
    }
    ~Listeners() {
        for (auto& [a, b] : blocks) {
            AudioObjectRemovePropertyListenerBlock(object, &a, dispatch_get_main_queue(), b);
            [b release];
        }
    }
};

} // namespace

// ---- PwNodeAudio ----

PwNodeAudio::PwNodeAudio(PwNode* node) : QObject(node), mNode(node) { refresh(); }

void PwNodeAudio::refresh() {
    const AudioObjectID dev = mNode->device();
    const bool sink = mNode->isSink();

    Float32 vol = 0;
    UInt32 size = sizeof(vol);
    auto va = volumeAddr(dev, sink);
    if (AudioObjectGetPropertyData(dev, &va, 0, nullptr, &size, &vol) == noErr && !qFuzzyCompare(qreal(vol) + 1, mVolume + 1)) {
        mVolume = vol;
        emit volumesChanged();
    }

    UInt32 mute = 0;
    size = sizeof(mute);
    auto ma = addr(kAudioDevicePropertyMute, scopeFor(sink));
    if (AudioObjectGetPropertyData(dev, &ma, 0, nullptr, &size, &mute) == noErr && bool(mute) != mMuted) {
        mMuted = mute;
        emit mutedChanged();
    }
}

void PwNodeAudio::setVolume(qreal v) {
    Float32 vol = Float32(std::clamp(v, 0.0, 1.0));
    auto va = volumeAddr(mNode->device(), mNode->isSink());
    AudioObjectSetPropertyData(mNode->device(), &va, 0, nullptr, sizeof(vol), &vol);
    refresh();
}

void PwNodeAudio::setMuted(bool m) {
    UInt32 mute = m;
    auto ma = addr(kAudioDevicePropertyMute, scopeFor(mNode->isSink()));
    AudioObjectSetPropertyData(mNode->device(), &ma, 0, nullptr, sizeof(mute), &mute);
    refresh();
}

// ---- PwNode ----

PwNode::PwNode(unsigned deviceId, bool sink, QObject* parent) : QObject(parent), mDevice(deviceId), mSink(sink) {
    mName = stringProp(deviceId, kAudioDevicePropertyDeviceUID);
    mDescription = stringProp(deviceId, kAudioObjectPropertyName);
    mAudio = new PwNodeAudio(this);

    auto* l = new Listeners{deviceId, {}};
    PwNodeAudio* audio = mAudio;
    const auto scope = scopeFor(sink);
    l->add(volumeAddr(deviceId, sink), ^{ audio->refresh(); });
    l->add(addr(kAudioDevicePropertyMute, scope), ^{ audio->refresh(); });
    mListener = l;
}

PwNode::~PwNode() { delete static_cast<Listeners*>(mListener); }

QVariantMap PwNode::properties() const {
    return {{"node.name", mName},
            {"node.description", mDescription},
            {"media.class", mSink ? "Audio/Sink" : "Audio/Source"}};
}

// ---- Pipewire ----

Pipewire::Pipewire(QObject* parent) : QObject(parent), mNodes(new ObjectModel(this)) {
    rescan();
    auto* l = new Listeners{kAudioObjectSystemObject, {}};
    l->add(addr(kAudioHardwarePropertyDevices), ^{ rescan(); });
    l->add(addr(kAudioHardwarePropertyDefaultOutputDevice), ^{ updateDefaults(); });
    l->add(addr(kAudioHardwarePropertyDefaultInputDevice), ^{ updateDefaults(); });
    mListener = l;
}

Pipewire::~Pipewire() { delete static_cast<Listeners*>(mListener); }

PwNode* Pipewire::find(unsigned device, bool sink) const {
    for (QObject* o : mNodes->values()) {
        auto* n = static_cast<PwNode*>(o);
        if (n->device() == device && n->isSink() == sink) return n;
    }
    return nullptr;
}

void Pipewire::rescan() {
    auto a = addr(kAudioHardwarePropertyDevices);
    UInt32 size = 0;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, nullptr, &size);
    std::vector<AudioObjectID> devices(size / sizeof(AudioObjectID));
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, devices.data());

    QList<PwNode*> keep;
    for (AudioObjectID dev : devices) {
        for (bool sink : {true, false}) {
            if (!hasStreams(dev, sink)) continue;
            PwNode* n = find(dev, sink);
            if (!n) {
                n = new PwNode(dev, sink, this);
                mNodes->append(n);
            }
            keep << n;
        }
    }
    for (QObject* o : mNodes->values()) {
        auto* n = static_cast<PwNode*>(o);
        if (keep.contains(n)) continue;
        if (n == mSink) mSink = nullptr;
        if (n == mSource) mSource = nullptr;
        mNodes->remove(n);
        n->deleteLater();
    }
    updateDefaults();
}

void Pipewire::updateDefaults() {
    if (PwNode* s = find(defaultDevice(true), true); s != mSink) {
        mSink = s;
        emit defaultAudioSinkChanged();
    }
    if (PwNode* s = find(defaultDevice(false), false); s != mSource) {
        mSource = s;
        emit defaultAudioSourceChanged();
    }
}

void Pipewire::setDefaultAudioSink(PwNode* n) {
    if (!n || !n->isSink()) return;
    AudioObjectID dev = n->device();
    auto a = addr(kAudioHardwarePropertyDefaultOutputDevice);
    AudioObjectSetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, sizeof(dev), &dev);
}

void Pipewire::setDefaultAudioSource(PwNode* n) {
    if (!n || n->isSink()) return;
    AudioObjectID dev = n->device();
    auto a = addr(kAudioHardwarePropertyDefaultInputDevice);
    AudioObjectSetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, sizeof(dev), &dev);
}
