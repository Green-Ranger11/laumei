#include "desktopentries.h"

#include <QDir>
#include <QProcess>
#include <QSet>

#import <Foundation/Foundation.h>

namespace {

// Where apps live; one level of subfolders (Utilities, vendor folders) too.
QStringList appRoots() {
    return {"/Applications", "/System/Applications", QDir::homePath() + "/Applications",
            "/System/Library/CoreServices/Applications"};
}

// LSApplicationCategoryType -> freedesktop main category, so configs that
// colour or group by Categories= keep working.
QString freedesktopCategory(const QString& uti) {
    const QString c = uti.section('.', -1);
    if (c == "developer-tools") return "Development";
    if (c == "music") return "Audio";
    if (c == "video" || c == "entertainment") return "Video";
    if (c == "graphics-design" || c == "photography") return "Graphics";
    if (c == "games" || c.endsWith("-games")) return "Game";
    if (c == "productivity" || c == "business" || c == "finance" || c == "education") return "Office";
    if (c == "utilities") return "System";
    if (c == "social-networking" || c == "news") return "Network";
    return {};
}

// "public.app-category.developer-tools" -> "Developer Tools"
QString humanCategory(const QString& uti) {
    QStringList words = uti.section('.', -1).split('-', Qt::SkipEmptyParts);
    for (QString& w : words) w[0] = w[0].toUpper();
    return words.join(' ');
}

QString str(id value) { return [value isKindOfClass:NSString.class] ? QString::fromNSString(value) : QString(); }

DesktopEntry* entryFor(const QString& path, QObject* parent) {
    NSBundle* bundle = [NSBundle bundleWithPath:path.toNSString()];
    if (!bundle) return nullptr;
    NSDictionary* info = bundle.infoDictionary;
    if ([info[@"LSBackgroundOnly"] boolValue]) return nullptr;

    auto* e = new DesktopEntry(parent);
    e->mBundlePath = path;
    e->mId = str(bundle.bundleIdentifier);
    if (e->mId.isEmpty()) e->mId = path;
    // Finder's display name: localized and without ".app".
    e->mName = QString::fromNSString([NSFileManager.defaultManager displayNameAtPath:path.toNSString()]);
    if (e->mName.endsWith(".app")) e->mName.chop(4);
    const QString category = str(info[@"LSApplicationCategoryType"]);
    if (!category.isEmpty()) {
        e->mGenericName = humanCategory(category);
        if (const QString fd = freedesktopCategory(category); !fd.isEmpty()) e->mCategories << fd;
    }
    e->mIcon = path; // resolved by the image://icon provider
    return e;
}

} // namespace

QString DesktopEntry::execString() const { return QString("open -a \"%1\"").arg(mBundlePath); }

QStringList DesktopEntry::command() const { return {"open", "-a", mBundlePath}; }

void DesktopEntry::execute() const { QProcess::startDetached("/usr/bin/open", {"-a", mBundlePath}); }

void DesktopEntryModel::setValues(const QList<QObject*>& v) {
    for (QObject* o : mValues) o->deleteLater();
    mValues = v;
    emit valuesChanged();
}

DesktopEntries::DesktopEntries(QObject* parent)
    : QObject(parent), mApplications(new DesktopEntryModel(this)) {
    // Installers touch these dirs several times in a row; rescan once.
    mRescan.setSingleShot(true);
    mRescan.setInterval(1000);
    connect(&mRescan, &QTimer::timeout, this, &DesktopEntries::scan);
    connect(&mWatcher, &QFileSystemWatcher::directoryChanged, &mRescan, qOverload<>(&QTimer::start));
    scan();
}

void DesktopEntries::scan() {
    QList<QObject*> found;
    QSet<QString> seen;
    QStringList watched;

    auto add = [&](const QString& path) {
        auto* e = entryFor(path, mApplications);
        if (!e) return;
        if (seen.contains(e->mId)) {
            delete e;
            return;
        }
        seen.insert(e->mId);
        found << e;
    };

    for (const QString& root : appRoots()) {
        QDir dir(root);
        if (!dir.exists()) continue;
        watched << root;
        for (const QFileInfo& fi : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (fi.suffix() == "app") add(fi.absoluteFilePath());
            else {
                watched << fi.absoluteFilePath();
                for (const QFileInfo& sub : QDir(fi.absoluteFilePath()).entryInfoList({"*.app"}, QDir::Dirs))
                    add(sub.absoluteFilePath());
            }
        }
    }
    add("/System/Library/CoreServices/Finder.app");

    if (!mWatcher.directories().isEmpty()) mWatcher.removePaths(mWatcher.directories());
    mWatcher.addPaths(watched);
    mApplications->setValues(found);
}

DesktopEntry* DesktopEntries::byId(const QString& id) const {
    for (QObject* o : mApplications->values()) {
        auto* e = static_cast<DesktopEntry*>(o);
        if (e->mId == id) return e;
    }
    return nullptr;
}

DesktopEntry* DesktopEntries::heuristicLookup(const QString& name) const {
    if (auto* e = byId(name)) return e;
    for (QObject* o : mApplications->values()) {
        auto* e = static_cast<DesktopEntry*>(o);
        if (e->mName.compare(name, Qt::CaseInsensitive) == 0 ||
            e->mId.section('.', -1).compare(name, Qt::CaseInsensitive) == 0 ||
            QFileInfo(e->mBundlePath).completeBaseName().compare(name, Qt::CaseInsensitive) == 0)
            return e;
    }
    return nullptr;
}
