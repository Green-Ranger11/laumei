#include "globalshortcut.h"

#include <Carbon/Carbon.h>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QPointer>

namespace {

QHash<quint32, QPointer<GlobalShortcut>>& shortcuts() {
    static QHash<quint32, QPointer<GlobalShortcut>> byId;
    return byId;
}

OSStatus onHotKey(EventHandlerCallRef, EventRef event, void*) {
    EventHotKeyID hk;
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(hk), nullptr, &hk);
    if (GlobalShortcut* s = shortcuts().value(hk.id)) {
        if (GetEventKind(event) == kEventHotKeyPressed) emit s->pressed();
        else emit s->released();
    }
    return noErr;
}

void installHandler() {
    static bool installed = false;
    if (installed) return;
    installed = true;
    const EventTypeSpec types[] = {{kEventClassKeyboard, kEventHotKeyPressed},
                                   {kEventClassKeyboard, kEventHotKeyReleased}};
    InstallApplicationEventHandler(&onHotKey, 2, types, nullptr, nullptr);
}

// "appid:name" or "name" -> chord, from the shortcuts file.
QString chordFor(const QString& appid, const QString& name) {
    const QString base = qEnvironmentVariable("XDG_CONFIG_HOME", QDir::homePath() + "/.config");
    QFile f(base + "/laumei/shortcuts");
    if (!f.open(QIODevice::ReadOnly)) return {};
    QString any;
    for (QString line : QString::fromUtf8(f.readAll()).split('\n')) {
        line = line.section('#', 0, 0).trimmed();
        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed(), chord = line.mid(eq + 1).trimmed();
        if (key == appid + ":" + name) return chord;
        if (key == name) any = chord;
    }
    return any;
}

const QHash<QString, quint32>& keyCodes() {
    static const QHash<QString, quint32> codes = {
        {"a", kVK_ANSI_A}, {"b", kVK_ANSI_B}, {"c", kVK_ANSI_C}, {"d", kVK_ANSI_D}, {"e", kVK_ANSI_E},
        {"f", kVK_ANSI_F}, {"g", kVK_ANSI_G}, {"h", kVK_ANSI_H}, {"i", kVK_ANSI_I}, {"j", kVK_ANSI_J},
        {"k", kVK_ANSI_K}, {"l", kVK_ANSI_L}, {"m", kVK_ANSI_M}, {"n", kVK_ANSI_N}, {"o", kVK_ANSI_O},
        {"p", kVK_ANSI_P}, {"q", kVK_ANSI_Q}, {"r", kVK_ANSI_R}, {"s", kVK_ANSI_S}, {"t", kVK_ANSI_T},
        {"u", kVK_ANSI_U}, {"v", kVK_ANSI_V}, {"w", kVK_ANSI_W}, {"x", kVK_ANSI_X}, {"y", kVK_ANSI_Y},
        {"z", kVK_ANSI_Z}, {"0", kVK_ANSI_0}, {"1", kVK_ANSI_1}, {"2", kVK_ANSI_2}, {"3", kVK_ANSI_3},
        {"4", kVK_ANSI_4}, {"5", kVK_ANSI_5}, {"6", kVK_ANSI_6}, {"7", kVK_ANSI_7}, {"8", kVK_ANSI_8},
        {"9", kVK_ANSI_9}, {"-", kVK_ANSI_Minus}, {"minus", kVK_ANSI_Minus}, {"=", kVK_ANSI_Equal},
        {"equal", kVK_ANSI_Equal}, {"[", kVK_ANSI_LeftBracket}, {"]", kVK_ANSI_RightBracket},
        {";", kVK_ANSI_Semicolon}, {"'", kVK_ANSI_Quote}, {",", kVK_ANSI_Comma}, {"comma", kVK_ANSI_Comma},
        {".", kVK_ANSI_Period}, {"period", kVK_ANSI_Period}, {"/", kVK_ANSI_Slash}, {"slash", kVK_ANSI_Slash},
        {"\\", kVK_ANSI_Backslash}, {"`", kVK_ANSI_Grave}, {"grave", kVK_ANSI_Grave},
        {"space", kVK_Space}, {"return", kVK_Return}, {"enter", kVK_Return}, {"tab", kVK_Tab},
        {"escape", kVK_Escape}, {"esc", kVK_Escape}, {"backspace", kVK_Delete}, {"delete", kVK_ForwardDelete},
        {"left", kVK_LeftArrow}, {"right", kVK_RightArrow}, {"up", kVK_UpArrow}, {"down", kVK_DownArrow},
        {"home", kVK_Home}, {"end", kVK_End}, {"pageup", kVK_PageUp}, {"pagedown", kVK_PageDown},
        {"f1", kVK_F1}, {"f2", kVK_F2}, {"f3", kVK_F3}, {"f4", kVK_F4}, {"f5", kVK_F5}, {"f6", kVK_F6},
        {"f7", kVK_F7}, {"f8", kVK_F8}, {"f9", kVK_F9}, {"f10", kVK_F10}, {"f11", kVK_F11}, {"f12", kVK_F12},
        {"f13", kVK_F13}, {"f14", kVK_F14}, {"f15", kVK_F15}, {"f16", kVK_F16}, {"f17", kVK_F17},
        {"f18", kVK_F18}, {"f19", kVK_F19}, {"f20", kVK_F20},
    };
    return codes;
}

} // namespace

bool GlobalShortcut::parseChord(const QString& chord, quint32* key, quint32* modifiers) {
    *modifiers = 0;
    const QStringList parts = chord.toLower().split('+');
    for (int i = 0; i < parts.size(); i++) {
        const QString p = parts[i].trimmed();
        // The key is last; "+" itself would split into empty parts.
        if (i == parts.size() - 1) {
            if (!keyCodes().contains(p)) return false;
            *key = keyCodes().value(p);
            return true;
        }
        if (p == "cmd" || p == "command" || p == "super") *modifiers |= cmdKey;
        else if (p == "ctrl" || p == "control") *modifiers |= controlKey;
        else if (p == "alt" || p == "opt" || p == "option") *modifiers |= optionKey;
        else if (p == "shift") *modifiers |= shiftKey;
        else return false;
    }
    return false;
}

void GlobalShortcut::componentComplete() {
    const QString chord = chordFor(mAppid, mName);
    if (chord.isEmpty()) {
        qInfo("GlobalShortcut %s:%s has no key; bind it in ~/.config/laumei/shortcuts", qPrintable(mAppid),
              qPrintable(mName));
        return;
    }
    quint32 key, modifiers;
    if (!parseChord(chord, &key, &modifiers)) {
        qWarning("GlobalShortcut %s: can't read \"%s\"", qPrintable(mName), qPrintable(chord));
        return;
    }
    installHandler();
    static quint32 nextId = 0;
    mId = ++nextId;
    EventHotKeyRef ref = nullptr;
    const OSStatus err = RegisterEventHotKey(key, modifiers, {'lmei', mId}, GetApplicationEventTarget(), 0, &ref);
    if (err != noErr) {
        qWarning("GlobalShortcut %s: macOS refused %s (%d; taken, or no cmd/ctrl)", qPrintable(mName),
                 qPrintable(chord), int(err));
        return;
    }
    mHotKey = ref;
    shortcuts().insert(mId, this);
}

GlobalShortcut::~GlobalShortcut() {
    if (mHotKey) UnregisterEventHotKey(static_cast<EventHotKeyRef>(mHotKey));
    shortcuts().remove(mId);
}
