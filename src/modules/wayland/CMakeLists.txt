qt_add_library(qs_wayland STATIC)
qt_add_qml_module(qs_wayland
    URI Quickshell.Wayland
    VERSION 1.0
    SOURCES
        layershell.cpp layershell.h
)
target_include_directories(qs_wayland PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qs_wayland PUBLIC Qt6::Core Qt6::Qml)
