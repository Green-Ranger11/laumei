#include "bluetooth.h"

#import <IOBluetooth/IOBluetooth.h>

// Controller power and discoverability have no public API; these IOBluetooth
// exports are what System Settings and blueutil use.
extern "C" {
int IOBluetoothPreferenceGetControllerPowerState(void);
void IOBluetoothPreferenceSetControllerPowerState(int state);
int IOBluetoothPreferenceGetDiscoverableState(void);
void IOBluetoothPreferenceSetDiscoverableState(int state);
}

namespace {

IOBluetoothDevice* io(void* p) { return static_cast<IOBluetoothDevice*>(p); }

// BlueZ-style "aa:bb:..." (IOBluetooth uses dashes).
QString addressOf(IOBluetoothDevice* d) {
    return QString::fromNSString(d.addressString).replace('-', ':').toUpper();
}

// Class of Device -> the freedesktop icon names BlueZ reports, which configs
// map to glyphs.
QString iconFor(IOBluetoothDevice* d) {
    const auto major = d.deviceClassMajor;
    const auto minor = d.deviceClassMinor;
    switch (major) {
    case kBluetoothDeviceClassMajorComputer: return "computer";
    case kBluetoothDeviceClassMajorPhone: return "phone";
    case kBluetoothDeviceClassMajorAudio:
        if (minor == kBluetoothDeviceClassMinorAudioHeadset || minor == kBluetoothDeviceClassMinorAudioHandsFree)
            return "audio-headset";
        if (minor == kBluetoothDeviceClassMinorAudioHeadphones) return "audio-headphones";
        return "audio-card";
    case kBluetoothDeviceClassMajorPeripheral:
        if ((minor & 0x30) == 0x10) return "input-keyboard";
        if ((minor & 0x30) == 0x20) return "input-mouse";
        if ((minor & 0x0f) == 0x01 || (minor & 0x0f) == 0x02) return "input-gaming";
        if ((minor & 0x0f) == 0x05) return "input-tablet";
        return "input-keyboard";
    case kBluetoothDeviceClassMajorImaging: return (minor & 0x20) ? "printer" : "camera-photo";
    case kBluetoothDeviceClassMajorWearable: return "audio-headphones";
    default: break;
    }
    // Bluetooth LE devices usually report no class; go by name.
    const QString n = QString::fromNSString(d.name ?: @"").toLower();
    auto has = [&](std::initializer_list<const char*> words) {
        for (const char* w : words)
            if (n.contains(QLatin1String(w))) return true;
        return false;
    };
    if (has({"keyboard", "keys", "nuphy", "keychron", "halo"})) return "input-keyboard";
    if (has({"mouse", "trackpad", "mx master", "mx anywhere", "mx ergo"})) return "input-mouse";
    if (has({"controller", "gamepad", "dualsense", "xbox"})) return "input-gaming";
    if (has({"airpods", "buds", "headphone", "headset", "earbud"})) return "audio-headset";
    if (has({"speaker", "soundlink", "boom"})) return "audio-card";
    if (has({"iphone", "phone", "pixel", "galaxy"})) return "phone";
    return {};
}

// Apple devices report battery through private properties; 0 means unknown.
qreal batteryOf(IOBluetoothDevice* d) {
    int best = 0;
    for (NSString* key : {@"batteryPercentSingle", @"batteryPercentCombined"}) {
        @try {
            id v = [d valueForKey:key];
            if ([v isKindOfClass:NSNumber.class]) best = std::max(best, [v intValue]);
        } @catch (NSException*) {
        }
    }
    return best > 0 ? best / 100.0 : -1;
}

} // namespace

// Connection and pairing callbacks for one device.
@interface QsBtDeviceDelegate : NSObject {
@public
    BluetoothDevice* owner;
}
@end

@implementation QsBtDeviceDelegate
- (void)connectionComplete:(IOBluetoothDevice*)device status:(IOReturn)status {
    if (owner) owner->connectFinished(status == kIOReturnSuccess);
}
- (void)devicePairingFinished:(id)sender error:(IOReturn)error {
    if (owner) owner->pairFinished(error == kIOReturnSuccess);
}
// Numeric comparison: accept, like a BlueZ setup with no agent ("just works").
- (void)devicePairingUserConfirmationRequest:(id)sender numericValue:(BluetoothNumericValue)value {
    [sender replyUserConfirmation:YES];
}
@end

@interface QsBtInquiryDelegate : NSObject <IOBluetoothDeviceInquiryDelegate> {
@public
    BluetoothAdapter* owner;
}
@end

@implementation QsBtInquiryDelegate
- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry*)sender device:(IOBluetoothDevice*)device {
    if (owner) owner->deviceSeen(device);
}
- (void)deviceInquiryDeviceNameUpdated:(IOBluetoothDeviceInquiry*)sender
                                device:(IOBluetoothDevice*)device
                      devicesRemaining:(uint32_t)remaining {
    if (owner) owner->deviceSeen(device);
}
- (void)deviceInquiryComplete:(IOBluetoothDeviceInquiry*)sender error:(IOReturn)error aborted:(BOOL)aborted {
    if (owner) owner->inquiryStopped();
}
@end

@interface QsBtObserver : NSObject {
@public
    BluetoothAdapter* owner;
}
@end

@implementation QsBtObserver
- (void)deviceConnected:(IOBluetoothUserNotification*)note device:(IOBluetoothDevice*)device {
    if (owner) owner->deviceSeen(device);
}
@end

// ---- BluetoothDevice ----

BluetoothDevice::BluetoothDevice(void* ioDevice, BluetoothAdapter* adapter)
    : QObject(adapter), mDevice([io(ioDevice) retain]), mAdapter(adapter) {
    auto* delegate = [[QsBtDeviceDelegate alloc] init];
    delegate->owner = this;
    mDelegate = delegate;
    mAddress = addressOf(io(mDevice));
    mIcon = iconFor(io(mDevice));
    refresh();
}

BluetoothDevice::~BluetoothDevice() {
    static_cast<QsBtDeviceDelegate*>(mDelegate)->owner = nullptr;
    if (mPairer) {
        [static_cast<IOBluetoothDevicePair*>(mPairer) setDelegate:nil];
        [static_cast<IOBluetoothDevicePair*>(mPairer) release];
    }
    [static_cast<QsBtDeviceDelegate*>(mDelegate) release];
    [io(mDevice) release];
}

void BluetoothDevice::setState(int s) {
    if (s == mState) return;
    mState = s;
    emit stateChanged();
}

void BluetoothDevice::refresh() {
    IOBluetoothDevice* d = io(mDevice);
    QString n = QString::fromNSString(d.name ?: @"");
    if (n.isEmpty()) n = mAddress;
    if (n != mName) {
        mName = n;
        emit nameChanged();
    }
    if (mState != BluetoothDeviceState::Connecting && mState != BluetoothDeviceState::Disconnecting)
        setState(d.isConnected ? BluetoothDeviceState::Connected : BluetoothDeviceState::Disconnected);
    if (bool p = d.isPaired; p != mPaired) {
        mPaired = p;
        emit pairedChanged();
    }
    if (qreal b = batteryOf(d); b != mBattery) {
        mBattery = b;
        emit batteryChanged();
    }
}

void BluetoothDevice::connect() {
    if (connected() || mState == BluetoothDeviceState::Connecting) return;
    setState(BluetoothDeviceState::Connecting);
    if ([io(mDevice) openConnection:static_cast<id>(mDelegate)] != kIOReturnSuccess) connectFinished(false);
}

void BluetoothDevice::connectFinished(bool) {
    setState(io(mDevice).isConnected ? BluetoothDeviceState::Connected : BluetoothDeviceState::Disconnected);
}

void BluetoothDevice::disconnect() {
    if (!connected()) return;
    setState(BluetoothDeviceState::Disconnecting);
    [io(mDevice) closeConnection];
    // No completion callback for closeConnection; the poll picks up the rest.
    setState(io(mDevice).isConnected ? BluetoothDeviceState::Connected : BluetoothDeviceState::Disconnected);
}

void BluetoothDevice::pair() {
    if (mPaired || mPairing) return;
    auto* pairer = [[IOBluetoothDevicePair pairWithDevice:io(mDevice)] retain];
    pairer.delegate = static_cast<id>(mDelegate);
    if ([pairer start] != kIOReturnSuccess) {
        [pairer release];
        return;
    }
    mPairer = pairer;
    mPairing = true;
    emit pairingChanged();
}

void BluetoothDevice::cancelPair() {
    if (!mPairer) return;
    [static_cast<IOBluetoothDevicePair*>(mPairer) stop];
    pairFinished(false);
}

void BluetoothDevice::pairFinished(bool) {
    if (mPairer) {
        [static_cast<IOBluetoothDevicePair*>(mPairer) setDelegate:nil];
        [static_cast<IOBluetoothDevicePair*>(mPairer) release];
        mPairer = nullptr;
    }
    mPairing = false;
    emit pairingChanged();
    refresh();
}

void BluetoothDevice::forget() {
    IOBluetoothDevice* d = io(mDevice);
    if (d.isConnected) [d closeConnection];
    // Private, but it's the only way to unpair; System Settings uses it.
    if ([d respondsToSelector:@selector(remove)]) [d performSelector:@selector(remove)];
    refresh();
    if (!mPaired && !mAdapter->discovering()) mAdapter->removeDevice(this);
}

// ---- BluetoothAdapter ----

BluetoothAdapter::BluetoothAdapter(QObject* parent) : QObject(parent), mDevices(new ObjectModel(this)) {
    auto* observer = [[QsBtObserver alloc] init];
    observer->owner = this;
    mObserver = observer;
    mConnectNotification = [[IOBluetoothDevice registerForConnectNotifications:observer
                                                                       selector:@selector(deviceConnected:device:)] retain];

    // Power state, pairings and disconnects have no notifications worth the
    // trouble; these reads are cheap.
    mPoll.setInterval(2000);
    QObject::connect(&mPoll, &QTimer::timeout, this, &BluetoothAdapter::refresh);
    mPoll.start();
    refresh();
}

BluetoothAdapter::~BluetoothAdapter() {
    setDiscovering(false);
    [static_cast<IOBluetoothUserNotification*>(mConnectNotification) unregister];
    [static_cast<IOBluetoothUserNotification*>(mConnectNotification) release];
    static_cast<QsBtObserver*>(mObserver)->owner = nullptr;
    [static_cast<QsBtObserver*>(mObserver) release];
}

BluetoothDevice* BluetoothAdapter::find(void* ioDevice) const {
    const QString addr = addressOf(io(ioDevice));
    for (QObject* o : mDevices->values()) {
        auto* d = static_cast<BluetoothDevice*>(o);
        if (d->address() == addr) return d;
    }
    return nullptr;
}

void BluetoothAdapter::deviceSeen(void* ioDevice) {
    if (auto* d = find(ioDevice)) d->refresh();
    else mDevices->append(new BluetoothDevice(ioDevice, this));
}

void BluetoothAdapter::removeDevice(BluetoothDevice* d) {
    mDevices->remove(d);
    d->deleteLater();
}

void BluetoothAdapter::refresh() {
    if (bool e = IOBluetoothPreferenceGetControllerPowerState() != 0; e != mEnabled) {
        mEnabled = e;
        emit enabledChanged();
        if (!e) setDiscovering(false);
    }
    if (bool d = IOBluetoothPreferenceGetDiscoverableState() != 0; d != mDiscoverable) {
        mDiscoverable = d;
        emit discoverableChanged();
    }

    for (IOBluetoothDevice* dev in IOBluetoothDevice.pairedDevices) deviceSeen(dev);
    // Unpaired leftovers from a finished scan go away, like BlueZ's cache
    // expiring; the picker hides them anyway once scanning stops.
    if (!mDiscovering) {
        for (QObject* o : mDevices->values()) {
            auto* d = static_cast<BluetoothDevice*>(o);
            d->refresh();
            if (!d->paired() && !d->pairing() && !d->connected()) removeDevice(d);
        }
    } else {
        for (QObject* o : mDevices->values()) static_cast<BluetoothDevice*>(o)->refresh();
    }
}

void BluetoothAdapter::setEnabled(bool e) {
    if (e == mEnabled) return;
    IOBluetoothPreferenceSetControllerPowerState(e ? 1 : 0);
    // The controller takes a moment; report the new state right away and
    // let the poll correct it if the switch failed.
    mEnabled = e;
    emit enabledChanged();
    if (!e) setDiscovering(false);
}

void BluetoothAdapter::setDiscoverable(bool d) {
    if (d == mDiscoverable) return;
    IOBluetoothPreferenceSetDiscoverableState(d ? 1 : 0);
    mDiscoverable = d;
    emit discoverableChanged();
}

void BluetoothAdapter::setDiscovering(bool d) {
    if (d == mDiscovering) return;
    if (d && !mEnabled) return;
    if (d) {
        auto* delegate = [[QsBtInquiryDelegate alloc] init];
        delegate->owner = this;
        auto* inquiry = [[IOBluetoothDeviceInquiry inquiryWithDelegate:delegate] retain];
        inquiry.updateNewDeviceNames = YES;
        if ([inquiry start] != kIOReturnSuccess) {
            [inquiry release];
            [delegate release];
            return;
        }
        mInquiry = inquiry;
        mInquiryDelegate = delegate;
    } else if (mInquiry) {
        auto* inquiry = static_cast<IOBluetoothDeviceInquiry*>(mInquiry);
        static_cast<QsBtInquiryDelegate*>(mInquiryDelegate)->owner = nullptr;
        [inquiry stop];
        [inquiry release];
        [static_cast<QsBtInquiryDelegate*>(mInquiryDelegate) release];
        mInquiry = mInquiryDelegate = nullptr;
    }
    mDiscovering = d;
    emit discoveringChanged();
    if (!d) refresh();
}

void BluetoothAdapter::inquiryStopped() {
    // An inquiry runs ~10s; keep scanning until told to stop.
    if (mDiscovering && mInquiry) [static_cast<IOBluetoothDeviceInquiry*>(mInquiry) start];
}

// ---- Bluetooth ----

Bluetooth::Bluetooth(QObject* parent)
    : QObject(parent), mAdapter(new BluetoothAdapter(this)), mAdapters(new ObjectModel(this)) {
    mAdapters->append(mAdapter);
}
