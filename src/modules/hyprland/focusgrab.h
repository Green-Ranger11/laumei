#pragma once

#include <QList>
#include <QObject>
#include <QtQml/qqmlregistration.h>

// Quickshell.Hyprland's focus grab: while active, a click outside `windows`
// (in another app, or on another of the shell's windows) clears it. Hyprland
// also hands the windows keyboard focus; here they keep their own focus rules.
class HyprlandFocusGrab : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QList<QObject*> windows MEMBER mWindows NOTIFY windowsChanged)

public:
    using QObject::QObject;
    ~HyprlandFocusGrab() override { setActive(false); }

    bool active() const { return mActive; }
    void setActive(bool active);

signals:
    void activeChanged();
    void windowsChanged();
    void cleared();

private:
    void clickedWindow(void* nsWindow);

    bool mActive = false;
    QList<QObject*> mWindows;
    void* mGlobalMonitor = nullptr;
    void* mLocalMonitor = nullptr;
};
