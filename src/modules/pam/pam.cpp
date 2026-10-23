#include "pam.h"

#include <QMetaObject>

bool PamContext::start() {
    mMessage = QStringLiteral("PAM authentication is not available in Laumei; macOS locks the screen itself.");
    // Queued, like a real conversation would answer, so handlers are set up.
    QMetaObject::invokeMethod(this, [this] {
        emit pamMessage();
        emit error(PamError::StartFailed);
    }, Qt::QueuedConnection);
    return false;
}

void PamContext::setActive(bool a) {
    if (a) start();
}
