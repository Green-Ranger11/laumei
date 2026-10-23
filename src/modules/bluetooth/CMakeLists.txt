qt_add_library(qs_bluetooth STATIC)
qt_add_qml_module(qs_bluetooth
    URI Quickshell.Bluetooth
    VERSION 1.0
    SOURCES
        bluetooth.mm bluetooth.h
)
target_include_directories(qs_bluetooth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qs_bluetooth PUBLIC Qt6::Core Qt6::Qml qs_io "-framework IOBluetooth" "-framework Foundation")
