#include "layershell.h"

void WlrLayershell::setLayer(int l) {
    if (l == mLayer) return;
    mLayer = l;
    emit layerChanged();
}

void WlrLayershell::setKeyboardFocus(int f) {
    if (f == mKeyboardFocus) return;
    mKeyboardFocus = f;
    emit keyboardFocusChanged();
}

void WlrLayershell::setNs(const QString& n) {
    if (n == mNamespace) return;
    mNamespace = n;
    emit namespaceChanged();
}
