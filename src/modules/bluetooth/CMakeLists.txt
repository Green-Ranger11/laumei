qt_add_library(laumei_bluetooth STATIC)
qt_add_qml_module(laumei_bluetooth
    URI Quickshell.Bluetooth
    VERSION 1.0
    SOURCES
        bluetooth.mm bluetooth.h
)
target_include_directories(laumei_bluetooth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_bluetooth PUBLIC Qt6::Core Qt6::Qml laumei_io "-framework IOBluetooth" "-framework Foundation")
