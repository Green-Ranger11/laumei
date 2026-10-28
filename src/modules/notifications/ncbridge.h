#pragma once

// Copies notifications that reach Notification Center into the shell's
// NotificationServer, by watching Notification Center's own database. Reading
// it needs Full Disk Access for Laumei; without it this logs once and stays off.
// Off unless LAUMEI_NC_BRIDGE=1.
namespace NcBridge {

// Starts watching (once per process). Only notifications delivered after
// this are copied.
void start();

} // namespace NcBridge
