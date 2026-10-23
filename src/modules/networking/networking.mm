#include "networking.h"

#include <QCoreApplication>
#include <QPointer>
#include <QProcess>

#import <CoreLocation/CoreLocation.h>
#import <CoreWLAN/CoreWLAN.h>

namespace {

CWInterface* wifiInterface() { return CWWiFiClient.sharedWiFiClient.interface; }

// RSSI in dBm -> 0..1, -90 (barely there) to -30 (next to the router).
qreal signalFromRssi(NSInteger rssi) { return std::clamp((rssi + 90) / 60.0, 0.0, 1.0); }

int securityOf(CWNetwork* n) {
    if ([n supportsSecurity:kCWSecurityWPA3Enterprise]) return WifiSecurityType::Wpa3SuiteB192;
    if ([n supportsSecurity:kCWSecurityWPA3Personal] || [n supportsSecurity:kCWSecurityWPA3Transition])
        return WifiSecurityType::Sae;
    if ([n supportsSecurity:kCWSecurityWPA2Enterprise] || [n supportsSecurity:kCWSecurityEnterprise])
        return WifiSecurityType::Wpa2Eap;
    if ([n supportsSecurity:kCWSecurityWPA2Personal] || [n supportsSecurity:kCWSecurityPersonal])
        return WifiSecurityType::Wpa2Psk;
    if ([n supportsSecurity:kCWSecurityWPAEnterprise] || [n supportsSecurity:kCWSecurityWPAEnterpriseMixed])
        return WifiSecurityType::WpaEap;
    if ([n supportsSecurity:kCWSecurityWPAPersonal] || [n supportsSecurity:kCWSecurityWPAPersonalMixed])
        return WifiSecurityType::WpaPsk;
    if ([n supportsSecurity:kCWSecurityDynamicWEP]) return WifiSecurityType::DynamicWep;
    if ([n supportsSecurity:kCWSecurityWEP]) return WifiSecurityType::StaticWep;
    if ([n supportsSecurity:kCWSecurityOWE] || [n supportsSecurity:kCWSecurityOWETransition])
        return WifiSecurityType::Owe;
    if ([n supportsSecurity:kCWSecurityNone]) return WifiSecurityType::Open;
    return WifiSecurityType::Unknown;
}

bool isSecured(int security) { return security != WifiSecurityType::Open && security != WifiSecurityType::Owe; }

} // namespace

// CoreWLAN events arrive on a private queue; bounce them to the Qt thread.
@interface QsWifiEvents : NSObject <CWEventDelegate> {
@public
    QPointer<Networking> owner;
}
@end

@implementation QsWifiEvents
- (void)bounce {
    QPointer<Networking> o = owner;
    if (o) QMetaObject::invokeMethod(o, [o] { if (o) o->wifiEvent(); }, Qt::QueuedConnection);
}
- (void)ssidDidChangeForWiFiInterfaceWithName:(NSString*)name { [self bounce]; }
- (void)powerStateDidChangeForWiFiInterfaceWithName:(NSString*)name { [self bounce]; }
- (void)linkDidChangeForWiFiInterfaceWithName:(NSString*)name { [self bounce]; }
@end

// ---- WifiNetwork ----

WifiNetwork::WifiNetwork(const QString& ssid, WifiDevice* device) : QObject(device), mDevice(device), mName(ssid) {}

void WifiNetwork::update(qreal signal, int security, bool connected, bool known) {
    if (!qFuzzyCompare(signal + 1, mSignal + 1)) { mSignal = signal; emit signalStrengthChanged(); }
    if (security != mSecurity) { mSecurity = security; emit securityChanged(); }
    if (connected != mConnected) { mConnected = connected; emit connectedChanged(); }
    if (known != mKnown) { mKnown = known; emit knownChanged(); }
}

void WifiNetwork::setStateChanging(bool c) {
    if (c == mStateChanging) return;
    mStateChanging = c;
    emit stateChangingChanged();
}

void WifiNetwork::connect() { join({}); }

void WifiNetwork::connectWithPsk(const QString& psk) { join(psk); }

// networksetup joins with the keychain's saved password when none is given,
// and saves a new one on success.
void WifiNetwork::join(const QString& password) {
    if (mStateChanging) return;
    setStateChanging(true);
    auto* p = new QProcess(this);
    QStringList args{"-setairportnetwork", mDevice->name(), mName};
    if (!password.isEmpty()) args << password;
    QObject::connect(p, &QProcess::finished, this, [this, p, password] {
        // It exits 0 either way; failures are reported on stdout.
        const QString out = QString::fromUtf8(p->readAllStandardOutput() + p->readAllStandardError());
        p->deleteLater();
        setStateChanging(false);
        mDevice->refresh();
        if (!out.contains("Failed") && !out.contains("Could not") && !out.contains("Error")) return;
        const bool needsSecret = isSecured(mSecurity) && (password.isEmpty() || out.contains("password", Qt::CaseInsensitive) ||
                                                          out.contains("-3900"));
        emit connectionFailed(needsSecret ? ConnectionFailReason::NoSecrets : ConnectionFailReason::WifiClientFailed);
    });
    p->start("/usr/sbin/networksetup", args);
}

void WifiNetwork::disconnect() {
    if (!mConnected) return;
    [wifiInterface() disassociate];
    mDevice->refresh();
}

void WifiNetwork::forget() {
    // `quoted form of` keeps the SSID a single shell word whatever it holds.
    auto* p = new QProcess(this);
    QObject::connect(p, &QProcess::finished, this, [this, p] {
        p->deleteLater();
        mDevice->refresh();
    });
    p->start("/usr/bin/osascript",
             {"-e", "on run argv",
              "-e", "do shell script \"/usr/sbin/networksetup -removepreferredwirelessnetwork \" & quoted form of "
                    "(item 1 of argv) & \" \" & quoted form of (item 2 of argv) with administrator privileges",
              "-e", "end run", mDevice->name(), mName});
}

// ---- WifiDevice ----

WifiDevice::WifiDevice(const QString& interfaceName, QObject* parent)
    : QObject(parent), mName(interfaceName), mNetworks(new ObjectModel(this)) {
    // NetworkManager rescans about every 10s while a client wants results.
    mScanTimer.setInterval(10000);
    QObject::connect(&mScanTimer, &QTimer::timeout, this, &WifiDevice::scan);
    refresh();
}

WifiNetwork* WifiDevice::find(const QString& ssid) const {
    for (QObject* o : mNetworks->values())
        if (static_cast<WifiNetwork*>(o)->name() == ssid) return static_cast<WifiNetwork*>(o);
    return nullptr;
}

void WifiDevice::setScannerEnabled(bool e) {
    if (e == mScannerEnabled) return;
    mScannerEnabled = e;
    emit scannerEnabledChanged();
    if (e) {
        scan();
        mScanTimer.start();
    } else {
        mScanTimer.stop();
    }
}

void WifiDevice::refresh() {
    CWInterface* iface = wifiInterface();
    const QString current = QString::fromNSString(iface.ssid ?: @"");
    const bool wasConnected = connected();
    mCurrentSsid = current;

    mKnown.clear();
    for (CWNetworkProfile* profile in iface.configuration.networkProfiles)
        if (profile.ssid) mKnown.insert(QString::fromNSString(profile.ssid));

    for (QObject* o : mNetworks->values()) {
        auto* n = static_cast<WifiNetwork*>(o);
        n->update(n->signalStrength(), n->security(), n->name() == mCurrentSsid, mKnown.contains(n->name()));
    }
    if (wasConnected != connected()) emit connectedChanged();
}

void WifiDevice::scan() {
    if (mScanning || !wifiInterface().powerOn) return;
    mScanning = true;
    QPointer<WifiDevice> self(this);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        QList<Seen> seen;
        @autoreleasepool {
            NSSet<CWNetwork*>* found = [wifiInterface() scanForNetworksWithName:nil error:nil];
            for (CWNetwork* n in found) {
                // No SSID: hidden network, or Location access not granted yet.
                if (!n.ssid.length) continue;
                seen.append({QString::fromNSString(n.ssid), signalFromRssi(n.rssiValue), securityOf(n)});
            }
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, seen] {
            if (self) self->applyScan(seen);
        }, Qt::QueuedConnection);
    });
}

void WifiDevice::applyScan(const QList<Seen>& seen) {
    mScanning = false;
    refresh();

    // Strongest BSS per SSID.
    QHash<QString, Seen> best;
    for (const Seen& s : seen)
        if (!best.contains(s.ssid) || best[s.ssid].signal < s.signal) best[s.ssid] = s;
    // The connected network sometimes drops out of a single scan; keep it.
    if (connected() && !best.contains(mCurrentSsid)) {
        if (auto* cur = find(mCurrentSsid))
            best[mCurrentSsid] = {mCurrentSsid, cur->signalStrength(), cur->security()};
    }

    for (QObject* o : mNetworks->values()) {
        auto* n = static_cast<WifiNetwork*>(o);
        if (!best.contains(n->name()) && !n->stateChanging()) {
            mNetworks->remove(n);
            n->deleteLater();
        }
    }
    for (const Seen& s : best) {
        WifiNetwork* n = find(s.ssid);
        if (!n) {
            n = new WifiNetwork(s.ssid, this);
            n->update(s.signal, s.security, s.ssid == mCurrentSsid, mKnown.contains(s.ssid));
            mNetworks->append(n);
        } else {
            n->update(s.signal, s.security, s.ssid == mCurrentSsid, mKnown.contains(s.ssid));
        }
    }
}

// ---- Networking ----

Networking::Networking(QObject* parent) : QObject(parent), mDevices(new ObjectModel(this)) {
    // Network names are location data on macOS: without this, scans and the
    // current SSID come back nameless. The first run shows a permission prompt.
    auto* location = [[CLLocationManager alloc] init];
    const CLAuthorizationStatus status = location.authorizationStatus;
    if (status == kCLAuthorizationStatusNotDetermined) [location requestWhenInUseAuthorization];
    else if (status != kCLAuthorizationStatusAuthorizedAlways)
        qWarning("Networking: no Location access, so Wi-Fi network names are hidden. Allow qs in "
                 "System Settings > Privacy & Security > Location Services.");
    mLocation = location;

    CWInterface* iface = wifiInterface();
    if (!iface) return;
    mWifi = new WifiDevice(QString::fromNSString(iface.interfaceName), this);
    mDevices->append(mWifi);
    mWifiEnabled = iface.powerOn;

    auto* events = [[QsWifiEvents alloc] init];
    events->owner = this;
    CWWiFiClient* client = CWWiFiClient.sharedWiFiClient;
    client.delegate = events;
    [client startMonitoringEventWithType:CWEventTypeSSIDDidChange error:nil];
    [client startMonitoringEventWithType:CWEventTypePowerDidChange error:nil];
    [client startMonitoringEventWithType:CWEventTypeLinkDidChange error:nil];
    mEventDelegate = events;
}

Networking::~Networking() {
    if (mEventDelegate) {
        [CWWiFiClient.sharedWiFiClient stopMonitoringAllEventsAndReturnError:nil];
        CWWiFiClient.sharedWiFiClient.delegate = nil;
        [static_cast<QsWifiEvents*>(mEventDelegate) release];
    }
    [static_cast<CLLocationManager*>(mLocation) release];
}

void Networking::setWifiEnabled(bool e) {
    if (!mWifi || e == mWifiEnabled) return;
    [wifiInterface() setPower:e error:nil];
    wifiEvent();
}

void Networking::wifiEvent() {
    const bool on = wifiInterface().powerOn;
    if (on != mWifiEnabled) {
        mWifiEnabled = on;
        emit wifiEnabledChanged();
    }
    if (mWifi) {
        mWifi->refresh();
        if (on && mWifi->scannerEnabled()) mWifi->scan();
    }
}
