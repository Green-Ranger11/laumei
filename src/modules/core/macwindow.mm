#include "macwindow.h"

#import <AppKit/AppKit.h>

#include <QHash>

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

} // namespace

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
