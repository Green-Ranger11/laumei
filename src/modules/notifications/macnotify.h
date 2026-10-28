#pragma once

#include <QVariantMap>

// A macOS notification posted by Laumei itself, for notify-send in a shell
// with no NotificationServer. Shown as Laumei's (asks for permission once).
namespace MacNotify {

// `fields` as for NotificationServer::post. Delivery is asynchronous; a
// refusal is logged.
void post(const QVariantMap& fields);

} // namespace MacNotify
