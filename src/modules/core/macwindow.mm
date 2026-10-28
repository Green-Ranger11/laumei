#include "macwindow.h"

#import <AppKit/AppKit.h>

#include <QCursor>
#include <QHash>
#include <QPointer>
#include <QQmlListReference>
#include <QQuickItem>
#include <QRegion>

namespace {

// WlrLayer values, mirrored from layershell.h.
enum { LayerBackground = 0, LayerBottom = 1, LayerTop = 2, LayerOverlay = 3 };
// WlrKeyboardFocus values.
enum { FocusNone = 0 };

NSRunningApplication* g_previousApp = nil;

NSWindow* nsWindow(QWindow* w) {
    if (!w) return nil;
    NSView* view = reinterpret_cast<NSView*>(w->winId());
    return view.window;
}

NSWindowLevel levelFor(int layer) {
    switch (layer) {
    case LayerBackground: return CGWindowLevelForKey(kCGDesktopWindowLevelKey) + 1;
    case LayerBottom: return CGWindowLevelForKey(kCGDesktopIconWindowLevelKey) + 1;
    case LayerTop: return NSStatusWindowLevel;
    default: return NSPopUpMenuWindowLevel; // above the menu bar and status bars
    }
}

void apply(NSWindow* w, int layer) {
    [w setLevel:levelFor(layer)];
    [w setHasShadow:NO];
    // Follow the user across Spaces like a layer-shell surface follows the
    // output, and stay out of Cmd-` window cycling and Mission Control.
    [w setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces |
                             NSWindowCollectionBehaviorStationary |
                             NSWindowCollectionBehaviorIgnoresCycle |
                             NSWindowCollectionBehaviorFullScreenAuxiliary];
}

// Configured windows and their layers. After a wake a bar was left on a
// single Space (fixed by recreating it), so the setup is applied again
// whenever the displays change.
QHash<QWindow*, int>& configured() {
    static QHash<QWindow*, int> windows;
    static bool observing = false;
    if (!observing) {
        observing = true;
        void (^reapply)(NSNotification*) = ^(NSNotification*) {
            // Give AppKit a moment to finish moving windows around.
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
                for (auto it = windows.cbegin(); it != windows.cend(); ++it)
                    if (NSWindow* w = nsWindow(it.key())) apply(w, it.value());
            });
        };
        [NSNotificationCenter.defaultCenter addObserverForName:NSApplicationDidChangeScreenParametersNotification
                                                        object:nil queue:NSOperationQueue.mainQueue usingBlock:reapply];
        [NSWorkspace.sharedWorkspace.notificationCenter addObserverForName:NSWorkspaceScreensDidWakeNotification
                                                                   object:nil queue:NSOperationQueue.mainQueue usingBlock:reapply];
    }
    return windows;
}

// A Region in window coordinates: its item's rect (or x/y/width/height),
// then each child region applied by its intersection mode.
QRegion regionOf(QObject* r) {
    QRect rect;
    if (auto* item = qobject_cast<QQuickItem*>(r->property("item").value<QObject*>()))
        rect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height())).toAlignedRect();
    else
        rect = QRectF(r->property("x").toReal(), r->property("y").toReal(), r->property("width").toReal(),
                      r->property("height").toReal())
                   .toAlignedRect();
    QRegion out(rect, r->property("shape").toInt() == 1 ? QRegion::Ellipse : QRegion::Rectangle);

    QQmlListReference children(r, "regions");
    for (qsizetype i = 0; i < children.count(); i++) {
        QObject* child = children.at(i);
        const QRegion c = regionOf(child);
        switch (child->property("intersection").toInt()) {
        case 1: out = out.subtracted(c); break;
        case 2: out = out.intersected(c); break;
        case 3: out = out.xored(c); break;
        default: out = out.united(c);
        }
    }
    return out;
}

// Masked windows and their regions. macOS has no input masks, so a window
// ignores the mouse while the cursor is outside its region, re-checked on
// every mouse move (ours and other apps').
QHash<QWindow*, QPointer<QObject>>& masked() {
    static QHash<QWindow*, QPointer<QObject>> windows;
    return windows;
}

void updateMasks() {
    const QPoint cursor = QCursor::pos();
    for (auto it = masked().cbegin(); it != masked().cend(); ++it) {
        NSWindow* w = nsWindow(it.key());
        if (!w || !it.value()) continue;
        const bool inside = regionOf(it.value()).contains(it.key()->mapFromGlobal(cursor));
        if (w.ignoresMouseEvents == inside) [w setIgnoresMouseEvents:!inside];
    }
}

void watchMouse() {
    static bool watching = false;
    if (watching) return;
    watching = true;
    const NSEventMask mask = NSEventMaskMouseMoved | NSEventMaskLeftMouseDragged | NSEventMaskRightMouseDragged |
                             NSEventMaskScrollWheel;
    [NSEvent addGlobalMonitorForEventsMatchingMask:mask handler:^(NSEvent*) { updateMasks(); }];
    [NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent*(NSEvent* e) {
        updateMasks();
        return e;
    }];
}

} // namespace

void QsMacWindow::setInputMask(QWindow* window, QObject* region) {
    if (!window) return;
    if (!region) {
        masked().remove(window);
        if (NSWindow* w = nsWindow(window)) [w setIgnoresMouseEvents:NO];
        return;
    }
    if (!masked().contains(window))
        QObject::connect(window, &QObject::destroyed, [window] { masked().remove(window); });
    masked().insert(window, region);
    watchMouse();
    updateMasks();
}

bool QsMacWindow::maskContains(QObject* region, qreal x, qreal y) const {
    return region && regionOf(region).contains(QPointF(x, y).toPoint());
}

void QsMacWindow::configure(QWindow* window, int layer, int keyboardFocus) {
    // Flags first: changing them makes Qt reapply its own window level.
    // Bars and other no-focus surfaces must not steal key status on click.
    window->setFlag(Qt::WindowDoesNotAcceptFocus, keyboardFocus == FocusNone);

    if (!configured().contains(window))
        QObject::connect(window, &QObject::destroyed, [window] { configured().remove(window); });
    configured().insert(window, layer);

    NSWindow* w = nsWindow(window);
    if (!w) return;
    apply(w, layer);
}

void QsMacWindow::activate(QWindow* window) {
    NSRunningApplication* front = NSWorkspace.sharedWorkspace.frontmostApplication;
    if (front && ![front isEqual:NSRunningApplication.currentApplication]) {
        [g_previousApp release];
        g_previousApp = [front retain];
    }
    [NSApp activateIgnoringOtherApps:YES];
    if (NSWindow* w = nsWindow(window)) [w makeKeyAndOrderFront:nil];
    window->requestActivate();
}

void QsMacWindow::restoreFocus() {
    if (!g_previousApp) return;
    if (NSApp.isActive) [g_previousApp activateWithOptions:0];
    [g_previousApp release];
    g_previousApp = nil;
}

void QsMacWindow::setSuppressed(QWindow* window, bool suppressed) {
    NSWindow* w = nsWindow(window);
    if (!w) return;
    if (suppressed) [w orderOut:nil];
    else if (window->isVisible()) [w orderFrontRegardless];
}

void QsMacWindow::makeAccessoryApp() {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
