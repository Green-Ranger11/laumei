#pragma once

#include <QString>

// The name macOS gives a display (NSScreen.localizedName, which is also
// QScreen::name()), from its CGDirectDisplayID. Empty if it isn't connected.
QString displayName(qint64 displayId);
