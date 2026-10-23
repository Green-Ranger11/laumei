#include "session.h"

#include <dlfcn.h>
#include <cstdlib>

#import <ApplicationServices/ApplicationServices.h>
#import <IOKit/pwr_mgt/IOPMLib.h>

bool QsSessionLock::lockScreen() {
    // SACLockScreenImmediate is what Control Center's "Lock Screen" calls.
    if (void* h = dlopen("/System/Library/PrivateFrameworks/login.framework/Versions/Current/login", RTLD_LAZY)) {
        if (auto fn = reinterpret_cast<int (*)()>(dlsym(h, "SACLockScreenImmediate"))) return fn() == 0;
    }
    return std::system("/usr/bin/pmset displaysleepnow") == 0;
}

void IdleInhibitor::setEnabled(bool e) {
    if (e == enabled()) return;
    if (e) {
        IOPMAssertionID id = 0;
        if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
                                        CFSTR("Laumei IdleInhibitor"), &id) == kIOReturnSuccess)
            mAssertion = id;
    } else {
        IOPMAssertionRelease(mAssertion);
        mAssertion = 0;
    }
    emit enabledChanged();
}

IdleMonitor::IdleMonitor(QObject* parent) : QObject(parent) {
    mTimer.setInterval(1000);
    connect(&mTimer, &QTimer::timeout, this, &IdleMonitor::poll);
    mTimer.start();
}

void IdleMonitor::poll() {
    if (!mEnabled || mTimeout <= 0) return;
    const double idleFor =
        CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType);
    const bool idle = idleFor >= mTimeout;
    if (idle == mIdle) return;
    mIdle = idle;
    emit isIdleChanged();
}
