qt_add_library(qs_hyprland STATIC)
qt_add_qml_module(qs_hyprland
    URI Quickshell.Hyprland
    VERSION 1.0
    SOURCES
        hyprland.cpp hyprland.h
)
target_include_directories(qs_hyprland PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qs_hyprland PUBLIC Qt6::Core Qt6::Qml qs_io)
