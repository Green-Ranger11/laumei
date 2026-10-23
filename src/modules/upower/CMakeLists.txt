qt_add_library(qs_upower STATIC)
qt_add_qml_module(qs_upower
    URI Quickshell.Services.UPower
    VERSION 1.0
    SOURCES
        upower.mm upower.h
)
target_include_directories(qs_upower PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qs_upower PUBLIC Qt6::Core Qt6::Qml "-framework IOKit" "-framework CoreFoundation")
