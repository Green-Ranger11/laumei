#include "menubar.h"

#import <AppKit/AppKit.h>

QsMacMenuBar::QsMacMenuBar(QObject* parent) : QObject(parent) {
    // macOS posts nothing when the auto-hidden menu bar slides in, so poll.
    mTimer.setInterval(120);
    connect(&mTimer, &QTimer::timeout, this, &QsMacMenuBar::poll);
    mTimer.start();
}

void QsMacMenuBar::poll() {
    // Only an auto-hidden menu bar comes and goes.
    const bool autoHide = [NSUserDefaults.standardUserDefaults boolForKey:@"_HIHideMenuBar"];
    bool revealed = false;
    if (autoHide) {
        // The menu bar is a WindowServer window at the main-menu level that
        // sits above the screen (negative y) while hidden. Window titles need
        // Screen Recording access, so match on owner, level and shape.
        CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListOptionAll,
                                                     kCGNullWindowID);
        const NSInteger level = CGWindowLevelForKey(kCGMainMenuWindowLevelKey);
        for (NSDictionary* w in (__bridge NSArray*)list) {
            if (![w[(id)kCGWindowOwnerName] isEqualToString:@"Window Server"]) continue;
            if ([w[(id)kCGWindowLayer] integerValue] != level) continue;
            CGRect b;
            if (!CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)w[(id)kCGWindowBounds], &b)) continue;
            if (b.size.height > 60 || b.size.width < 300) continue;
            if (b.origin.y >= 0) {
                revealed = true;
                break;
            }
        }
        CFRelease(list);
    }
    if (revealed == mRevealed) return;
    mRevealed = revealed;
    emit revealedChanged();
}
