#include "ncbridge.h"

#import <AppKit/AppKit.h>
#include <QDir>
#include <QTimer>
#include <sqlite3.h>

#include "notifications.h"

namespace {

// shortcut: usernoted's private schema (macOS 15+ location); if Apple changes it
// this finds nothing and logs, and needs updating.
QString dbPath() {
    return QDir::homePath() + "/Library/Group Containers/group.com.apple.usernoted/db2/db";
}

// A text field is usually a string; localized ones are [key, key, format args],
// whose key we show as is (the strings live in the posting app).
QString str(id value) {
    if ([value isKindOfClass:NSArray.class] && [value count] == 3 && [value[2] isKindOfClass:NSArray.class]) {
        NSString* text = [value[0] isKindOfClass:NSString.class] ? value[0] : @"";
        for (id arg in value[2])
            if (NSRange at = [text rangeOfString:@"%@"]; at.location != NSNotFound)
                text = [text stringByReplacingCharactersInRange:at withString:[arg description]];
        return QString::fromNSString(text);
    }
    return [value isKindOfClass:NSString.class] ? QString::fromNSString(value) : QString();
}

// One `record` row's data: a binary plist with the bundle id under "app" and
// the content under "req" (titl, subt, body).
QVariantMap fieldsFor(const void* blob, int size) {
    NSData* data = [NSData dataWithBytesNoCopy:const_cast<void*>(blob) length:size freeWhenDone:NO];
    NSDictionary* rec = [NSPropertyListSerialization propertyListWithData:data options:0 format:nil error:nil];
    if (![rec isKindOfClass:NSDictionary.class]) return {};
    NSDictionary* req = rec[@"req"];
    if (![req isKindOfClass:NSDictionary.class]) return {};

    const QString bundleId = str(rec[@"app"]);
    QString appName = bundleId, appIcon;
    if (NSURL* url = [NSWorkspace.sharedWorkspace URLForApplicationWithBundleIdentifier:bundleId.toNSString()]) {
        appName = QString::fromNSString([NSFileManager.defaultManager displayNameAtPath:url.path]);
        if (appName.endsWith(".app")) appName.chop(4);
        appIcon = QString::fromNSString(url.path);
    }
    QString body = str(req[@"body"]);
    if (const QString subtitle = str(req[@"subt"]); !subtitle.isEmpty())
        body = body.isEmpty() ? subtitle : subtitle + "\n" + body;
    // The hint tells configs macOS already showed its own banner for this one.
    return {{"appName", appName},
            {"appIcon", appIcon},
            {"desktopEntry", bundleId},
            // osascript's notifications come under Script Editor; opening it is never
            // where they lead.
            {"bundleId", bundleId == "com.apple.scripteditor2" ? QString() : bundleId},
            {"summary", str(req[@"titl"])},
            {"body", body},
            {"hints", QVariantMap{{"x-laumei-notification-center", true}}}};
}

class Bridge {
public:
    Bridge() {
        if (sqlite3_open_v2(dbPath().toUtf8().constData(), &mDb, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK ||
            !query("select coalesce(max(rec_id), 0) from record", [this](sqlite3_stmt* s) {
                mLastId = sqlite3_column_int64(s, 0);
            })) {
            qInfo("NotificationServer: can't read Notification Center's database; give Laumei Full Disk Access "
                  "(System Settings > Privacy & Security) to show other apps' notifications.");
            return;
        }
        qInfo("NotificationServer: copying Notification Center (after record %lld)", mLastId);
        mTimer.setInterval(1000);
        QObject::connect(&mTimer, &QTimer::timeout, [this] { poll(); });
        mTimer.start();
    }

private:
    template <typename F> bool query(const char* sql, F row) {
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(mDb, sql, -1, &s, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int64(s, 1, mLastId);
        int rc;
        while ((rc = sqlite3_step(s)) == SQLITE_ROW) row(s);
        sqlite3_finalize(s);
        return rc == SQLITE_DONE;
    }

    void poll() {
        const bool ok = query("select rec_id, data from record where rec_id > ? order by rec_id", [this](sqlite3_stmt* s) {
            mLastId = sqlite3_column_int64(s, 0);
            const QVariantMap fields = fieldsFor(sqlite3_column_blob(s, 1), sqlite3_column_bytes(s, 1));
            if (!fields.isEmpty()) NotificationServer::post(fields);
        });
        if (!ok && !mWarned) {
            mWarned = true;
            qWarning("NotificationServer: reading Notification Center's database failed: %s", sqlite3_errmsg(mDb));
        }
    }

    sqlite3* mDb = nullptr;
    sqlite3_int64 mLastId = 0;
    bool mWarned = false;
    QTimer mTimer;
};

} // namespace

namespace NcBridge {

void start() {
    // Opt-in: macOS shows its own banners anyway, and only those open the
    // exact message when clicked.
    if (qgetenv("LAUMEI_NC_BRIDGE") != "1") return;
    static Bridge* bridge = new Bridge; // lives for the process, across reloads
    Q_UNUSED(bridge);
}

} // namespace NcBridge
