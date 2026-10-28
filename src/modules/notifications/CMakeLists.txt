qt_add_library(laumei_notifications STATIC)
qt_add_qml_module(laumei_notifications
    URI Quickshell.Services.Notifications
    VERSION 1.0
    SOURCES
        notifications.cpp notifications.h
        ncbridge.mm ncbridge.h
        macnotify.mm macnotify.h
)
target_include_directories(laumei_notifications PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_notifications PUBLIC Qt6::Core Qt6::Qml laumei_io sqlite3 "-framework AppKit" "-framework UserNotifications")
