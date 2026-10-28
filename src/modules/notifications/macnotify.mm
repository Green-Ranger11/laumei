#include "macnotify.h"

#import <UserNotifications/UserNotifications.h>

// Without a delegate macOS drops notifications while Laumei is the active app
// (e.g. a focusable panel is open).
@interface LaumeiNotifyDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation LaumeiNotifyDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))handler {
    handler(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList |
            UNNotificationPresentationOptionSound);
}
@end

namespace MacNotify {

void post(const QVariantMap& fields) {
    UNUserNotificationCenter* center = UNUserNotificationCenter.currentNotificationCenter;
    static LaumeiNotifyDelegate* delegate = [] {
        auto* d = [LaumeiNotifyDelegate new];
        UNUserNotificationCenter.currentNotificationCenter.delegate = d;
        return d;
    }();
    Q_UNUSED(delegate);

    UNMutableNotificationContent* content = [[UNMutableNotificationContent new] autorelease];
    content.title = fields.value("summary").toString().toNSString();
    content.subtitle = fields.value("appName").toString().toNSString();
    content.body = fields.value("body").toString().toNSString();
    if (fields.value("urgency").toInt() == 2) content.interruptionLevel = UNNotificationInterruptionLevelTimeSensitive;
    UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:NSUUID.UUID.UUIDString
                                                                          content:content
                                                                          trigger:nil];

    [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                          completionHandler:^(BOOL granted, NSError* error) {
                              if (!granted) {
                                  qWarning("notify-send: Laumei may not post notifications (%s); allow it in "
                                           "System Settings > Notifications.",
                                           error ? error.localizedDescription.UTF8String : "denied");
                                  return;
                              }
                              [center addNotificationRequest:request withCompletionHandler:^(NSError* e) {
                                  if (e) qWarning("notify-send: %s", e.localizedDescription.UTF8String);
                              }];
                          }];
}

} // namespace MacNotify
