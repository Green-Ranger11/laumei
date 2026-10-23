#pragma once

#include <QObject>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

// Native NSWindow tweaks Qt doesn't expose. Used by PanelWindow.qml; not part
// of Quickshell's API.
class QsMacWindow : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // `layer` and `keyboardFocus` take WlrLayer / WlrKeyboardFocus values.
    // Sets the window level, shows it on every Space and drops the shadow.
    Q_INVOKABLE void configure(QWindow* window, int layer, int keyboardFocus);

    // Takes keyboard focus for `window`, remembering the app that had it.
    Q_INVOKABLE void activate(QWindow* window);

    // Hands focus back to the app `activate` took it from.
    Q_INVOKABLE void restoreFocus();

    // Takes the window off screen (or puts it back) without Qt's visibility
    // changing, for a bar stepping aside while the native menu bar shows.
    Q_INVOKABLE void setSuppressed(QWindow* window, bool suppressed);

    // No Dock icon or menu bar; called once at startup.
    static void makeAccessoryApp();
};
