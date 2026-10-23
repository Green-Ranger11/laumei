#pragma once

#include <QQuickImageProvider>

// image://icon/<name>, the scheme Quickshell.iconPath() returns. A name can
// be a path (an .app bundle or any file: its Finder icon) or an app's name or
// bundle id. Freedesktop theme names have no macOS equivalent and resolve to
// nothing.
class IconProvider : public QQuickImageProvider {
public:
    IconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    // Filesystem path whose icon stands for `name`, or "" if there is none.
    static QString resolve(const QString& name);
};
