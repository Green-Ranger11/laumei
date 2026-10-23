#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Layer-shell has no macOS equivalent; PanelWindow maps these onto window
// levels and focus. Values match Quickshell's enums.
class WlrLayer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("WlrLayer is an enum.")

public:
    enum Enum { Background = 0, Bottom = 1, Top = 2, Overlay = 3 };
    Q_ENUM(Enum)
};

class WlrKeyboardFocus : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("WlrKeyboardFocus is an enum.")

public:
    enum Enum { None = 0, Exclusive = 1, OnDemand = 2 };
    Q_ENUM(Enum)
};

// WlrLayershell.layer / .keyboardFocus / .namespace on a PanelWindow.
class WlrLayershell : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use WlrLayershell as an attached property of PanelWindow.")
    QML_ATTACHED(WlrLayershell)
    Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)
    Q_PROPERTY(int keyboardFocus READ keyboardFocus WRITE setKeyboardFocus NOTIFY keyboardFocusChanged)
    Q_PROPERTY(QString namespace READ ns WRITE setNs NOTIFY namespaceChanged)

public:
    using QObject::QObject;
    static WlrLayershell* qmlAttachedProperties(QObject* object) { return new WlrLayershell(object); }

    int layer() const { return mLayer; }
    void setLayer(int l);
    int keyboardFocus() const { return mKeyboardFocus; }
    void setKeyboardFocus(int f);
    QString ns() const { return mNamespace; }
    void setNs(const QString& n);

signals:
    void layerChanged();
    void keyboardFocusChanged();
    void namespaceChanged();

private:
    int mLayer = WlrLayer::Top;
    int mKeyboardFocus = WlrKeyboardFocus::None;
    QString mNamespace = QStringLiteral("quickshell");
};
