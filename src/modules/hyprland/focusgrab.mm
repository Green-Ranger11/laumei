#include "focusgrab.h"

#import <AppKit/AppKit.h>
#include <QWindow>

namespace {
const NSEventMask kClicks = NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown | NSEventMaskOtherMouseDown;
}

void HyprlandFocusGrab::setActive(bool active) {
    if (active == mActive) return;
    mActive = active;
    if (active) {
        // Another app's click is always outside; ours depends on the window.
        mGlobalMonitor = [[NSEvent addGlobalMonitorForEventsMatchingMask:kClicks
                                                                 handler:^(NSEvent*) { clickedWindow(nullptr); }] retain];
        mLocalMonitor = [[NSEvent addLocalMonitorForEventsMatchingMask:kClicks
                                                               handler:^NSEvent*(NSEvent* e) {
                                                                   clickedWindow(e.window);
                                                                   return e;
                                                               }] retain];
    } else {
        for (void** monitor : {&mGlobalMonitor, &mLocalMonitor}) {
            if (!*monitor) continue;
            [NSEvent removeMonitor:(id)*monitor];
            [(id)*monitor release];
            *monitor = nullptr;
        }
    }
    emit activeChanged();
}

void HyprlandFocusGrab::clickedWindow(void* clicked) {
    for (QObject* o : mWindows) {
        auto* w = qobject_cast<QWindow*>(o);
        if (clicked && w && w->handle() && reinterpret_cast<NSView*>(w->winId()).window == clicked) return;
    }
    // Queued: the handler is mid-event, and clearing tends to hide windows.
    QMetaObject::invokeMethod(this, [this] {
        if (!mActive) return;
        setActive(false);
        emit cleared();
    }, Qt::QueuedConnection);
}
