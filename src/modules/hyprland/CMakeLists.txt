qt_add_library(laumei_hyprland STATIC)
qt_add_qml_module(laumei_hyprland
    URI Quickshell.Hyprland
    VERSION 1.0
    SOURCES
        hyprland.cpp hyprland.h
        displaynames.mm displaynames.h
        focusgrab.mm focusgrab.h
        globalshortcut.mm globalshortcut.h
)
target_include_directories(laumei_hyprland PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_hyprland PUBLIC Qt6::Core Qt6::Gui Qt6::Qml laumei_io "-framework AppKit" "-framework Carbon")
