#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QtQml/qqmlregistration.h>

// Quickshell.Hyprland's GlobalShortcut. Hyprland binds the keys in its own
// config; here they come from $XDG_CONFIG_HOME/laumei/shortcuts (default
// ~/.config/laumei/shortcuts), one per line:
//
//   launcher = cmd+space            # any appid
//   myshell:clipboard = ctrl+alt+v  # only appid "myshell"
//
// and are registered as system-wide hot keys. Modifiers: cmd (super), ctrl,
// alt (opt), shift; macOS refuses chords without cmd or ctrl. Keys by US
// layout position.
class GlobalShortcut : public QObject, public QQmlParserStatus {
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString name MEMBER mName NOTIFY changed)
    Q_PROPERTY(QString description MEMBER mDescription NOTIFY changed)
    Q_PROPERTY(QString appid MEMBER mAppid NOTIFY changed)
    Q_PROPERTY(QString triggerDescription MEMBER mTrigger NOTIFY changed)

public:
    using QObject::QObject;
    ~GlobalShortcut() override;

    void classBegin() override {}
    void componentComplete() override;

    // "cmd+shift+k" as a Carbon key code and modifier mask; false if
    // malformed. Public for the parser test.
    static bool parseChord(const QString& chord, quint32* key, quint32* modifiers);

signals:
    void changed();
    void pressed();
    void released();

private:
    QString mName, mDescription, mAppid = "quickshell", mTrigger;
    void* mHotKey = nullptr;
    quint32 mId = 0;
};
