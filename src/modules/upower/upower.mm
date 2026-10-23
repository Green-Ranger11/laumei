#include "upower.h"

#include <CoreFoundation/CoreFoundation.h>
#include <QMetaObject>
#include <QPointer>

#import <Foundation/Foundation.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

namespace {

int intOf(CFDictionaryRef d, CFStringRef key, int fallback = -1) {
    auto n = static_cast<CFNumberRef>(CFDictionaryGetValue(d, key));
    int v = fallback;
    if (n && CFGetTypeID(n) == CFNumberGetTypeID()) CFNumberGetValue(n, kCFNumberIntType, &v);
    return v;
}

bool boolOf(CFDictionaryRef d, CFStringRef key) {
    auto b = static_cast<CFBooleanRef>(CFDictionaryGetValue(d, key));
    return b && CFGetTypeID(b) == CFBooleanGetTypeID() && CFBooleanGetValue(b);
}

QString stringOf(CFDictionaryRef d, CFStringRef key) {
    auto s = static_cast<CFStringRef>(CFDictionaryGetValue(d, key));
    return s && CFGetTypeID(s) == CFStringGetTypeID() ? QString::fromCFString(s) : QString();
}

void changedCallback(void* context) { static_cast<UPower*>(context)->refresh(); }

} // namespace

QString UPowerDevice::iconName() const {
    if (!mPresent) return {};
    const int level = int(mPercentage * 10) * 10;
    const bool charging = mState == UPowerDeviceState::Charging || mState == UPowerDeviceState::FullyCharged;
    return QString("battery-%1%2").arg(level, 3, 10, QChar('0')).arg(charging ? "-charging" : "");
}

void UPowerDevice::update(bool present, qreal pct, int state, qreal toEmpty, qreal toFull, qreal health,
                          const QString& model) {
    if (present == mPresent && pct == mPercentage && state == mState && toEmpty == mTimeToEmpty &&
        toFull == mTimeToFull && health == mHealth && model == mModel)
        return;
    mPresent = present;
    mPercentage = pct;
    mState = state;
    mTimeToEmpty = toEmpty;
    mTimeToFull = toFull;
    mHealth = health;
    mModel = model;
    emit changed();
}

PowerProfiles::PowerProfiles(QObject* parent) : QObject(parent) {
    refresh();
    QPointer<PowerProfiles> self(this);
    id token = [NSNotificationCenter.defaultCenter
        addObserverForName:NSProcessInfoPowerStateDidChangeNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification*) {
                    QMetaObject::invokeMethod(self, [self] { if (self) self->refresh(); }, Qt::QueuedConnection);
                }];
    mObserver = [token retain]; // manual retain/release: this file is not ARC
}

PowerProfiles::~PowerProfiles() {
    if (!mObserver) return;
    [NSNotificationCenter.defaultCenter removeObserver:static_cast<id>(mObserver)];
    [static_cast<id>(mObserver) release];
}

void PowerProfiles::refresh() {
    const int p = NSProcessInfo.processInfo.lowPowerModeEnabled ? PowerProfile::PowerSaver : PowerProfile::Balanced;
    if (p == mProfile) return;
    mProfile = p;
    emit profileChanged();
}

void PowerProfiles::setProfile(int p) {
    if (p == mProfile || mWarned) return;
    mWarned = true;
    qWarning("PowerProfiles: switching Low Power Mode needs administrator rights; ignoring.");
}

UPower::UPower(QObject* parent) : QObject(parent), mDevice(new UPowerDevice(this)) {
    refresh();
    // Fires on plug/unplug and every percentage step.
    CFRunLoopSourceRef src = IOPSNotificationCreateRunLoopSource(changedCallback, this);
    if (src) {
        CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopDefaultMode);
        mSource = src;
    }
}

UPower::~UPower() {
    if (mSource) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), static_cast<CFRunLoopSourceRef>(mSource), kCFRunLoopDefaultMode);
        CFRelease(static_cast<CFRunLoopSourceRef>(mSource));
    }
}

void UPower::refresh() {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    CFArrayRef list = info ? IOPSCopyPowerSourcesList(info) : nullptr;

    bool present = false, onBattery = false;
    qreal pct = 0, toEmpty = 0, toFull = 0, health = 0;
    int state = UPowerDeviceState::Unknown;
    QString model;

    for (CFIndex i = 0; list && i < CFArrayGetCount(list); i++) {
        CFDictionaryRef d = IOPSGetPowerSourceDescription(info, CFArrayGetValueAtIndex(list, i));
        if (!d || stringOf(d, CFSTR(kIOPSTypeKey)) != QLatin1String(kIOPSInternalBatteryType)) continue;
        present = true;
        const int cur = intOf(d, CFSTR(kIOPSCurrentCapacityKey), 0);
        const int max = intOf(d, CFSTR(kIOPSMaxCapacityKey), 100);
        pct = max > 0 ? qreal(cur) / max : 0;
        onBattery = stringOf(d, CFSTR(kIOPSPowerSourceStateKey)) == QLatin1String(kIOPSBatteryPowerValue);
        const bool charging = boolOf(d, CFSTR(kIOPSIsChargingKey));
        const bool charged = boolOf(d, CFSTR(kIOPSIsChargedKey));
        state = onBattery ? UPowerDeviceState::Discharging
              : charging ? UPowerDeviceState::Charging
              : (charged || cur >= max) ? UPowerDeviceState::FullyCharged
              : UPowerDeviceState::PendingCharge;
        // Minutes, -1 while macOS is still estimating; UPower uses seconds.
        const int empty = intOf(d, CFSTR(kIOPSTimeToEmptyKey));
        const int full = intOf(d, CFSTR(kIOPSTimeToFullChargeKey));
        toEmpty = onBattery && empty > 0 ? empty * 60 : 0;
        toFull = charging && full > 0 ? full * 60 : 0;
        model = stringOf(d, CFSTR(kIOPSNameKey));
        const QString h = stringOf(d, CFSTR(kIOPSBatteryHealthKey));
        health = h == "Good" ? 1.0 : h == "Fair" ? 0.8 : h == "Poor" ? 0.6 : 0;
        break;
    }
    if (list) CFRelease(list);
    if (info) CFRelease(info);

    mDevice->update(present, pct, state, toEmpty, toFull, health, model);
    if (onBattery != mOnBattery) {
        mOnBattery = onBattery;
        emit onBatteryChanged();
    }
}
