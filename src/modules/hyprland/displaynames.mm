#include "displaynames.h"

#import <AppKit/AppKit.h>

QString displayName(qint64 displayId) {
    for (NSScreen* s in NSScreen.screens) {
        NSNumber* n = s.deviceDescription[@"NSScreenNumber"];
        if (n && n.longLongValue == displayId) return QString::fromNSString(s.localizedName);
    }
    return {};
}
