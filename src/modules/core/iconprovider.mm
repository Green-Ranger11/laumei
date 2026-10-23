#include "iconprovider.h"

#include <QFileInfo>
#include <QUrl>

#import <AppKit/AppKit.h>

QString IconProvider::resolve(const QString& name) {
    if (name.isEmpty()) return {};
    QString path = name.startsWith("file:") ? QUrl(name).toLocalFile() : name;
    if (path.startsWith('/')) return QFileInfo::exists(path) ? path : QString();

    NSWorkspace* ws = NSWorkspace.sharedWorkspace;
    if (NSURL* url = [ws URLForApplicationWithBundleIdentifier:name.toNSString()])
        return QString::fromNSString(url.path);
    for (const QString& dir : {"/Applications", "/System/Applications", "/System/Applications/Utilities",
                               "/Applications/Utilities"}) {
        const QString app = dir + "/" + name + ".app";
        if (QFileInfo::exists(app)) return app;
    }
    return {};
}

QImage IconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    // The id arrives percent-encoded when it came through a URL.
    const QString path = resolve(QUrl::fromPercentEncoding(id.toUtf8()));
    const int w = requestedSize.width() > 0 ? requestedSize.width() : 128;
    const int h = requestedSize.height() > 0 ? requestedSize.height() : w;
    if (size) *size = QSize(w, h);
    if (path.isEmpty()) return {};

    @autoreleasepool {
        NSImage* icon = [NSWorkspace.sharedWorkspace iconForFile:path.toNSString()];
        if (!icon) return {};

        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef ctx = CGBitmapContextCreate(img.bits(), w, h, 8, img.bytesPerLine(), cs,
                                                 kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
        CGColorSpaceRelease(cs);
        if (!ctx) return {};

        NSGraphicsContext* gc = [NSGraphicsContext graphicsContextWithCGContext:ctx flipped:NO];
        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext.currentContext = gc;
        [icon drawInRect:NSMakeRect(0, 0, w, h) fromRect:NSZeroRect operation:NSCompositingOperationSourceOver
                fraction:1.0];
        [NSGraphicsContext restoreGraphicsState];
        CGContextRelease(ctx);
        return img;
    }
}
