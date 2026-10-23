qt_add_library(laumei_systemtray STATIC)
qt_add_qml_module(laumei_systemtray
    URI Quickshell.Services.SystemTray
    VERSION 1.0
    SOURCES
        systemtray.h
)
target_include_directories(laumei_systemtray PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_systemtray PUBLIC Qt6::Core Qt6::Qml laumei_io)
