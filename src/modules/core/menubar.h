#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Whether the auto-hidden native menu bar is currently slid down (mouse at
// the top edge). PanelWindows on the top edge step aside while it is.
class QsMacMenuBar : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool revealed READ revealed NOTIFY revealedChanged)

public:
    explicit QsMacMenuBar(QObject* parent = nullptr);
    bool revealed() const { return mRevealed; }

signals:
    void revealedChanged();

private:
    void poll();

    QTimer mTimer;
    bool mRevealed = false;
};
