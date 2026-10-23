set_source_files_properties(Quickshell.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(laumei_core STATIC)
qt_add_qml_module(laumei_core
    URI Quickshell
    VERSION 1.0
    SOURCES
        qsnative.cpp qsnative.h
        desktopentries.mm desktopentries.h
        iconprovider.mm iconprovider.h
        macwindow.mm macwindow.h
        menubar.mm menubar.h
    QML_FILES
        Quickshell.qml
        ShellRoot.qml
        Scope.qml
        Singleton.qml
        Variants.qml
        PanelWindow.qml
        FloatingWindow.qml
        PanelAnchors.qml
        PanelMargins.qml
        Region.qml
        SystemClock.qml
        Edges.qml
        PopupAnchor.qml
        PopupWindow.qml
    DEPENDENCIES
        QtQuick
        Quickshell.Wayland
)
target_include_directories(laumei_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_core PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick laumei_io laumei_wayland
    "-framework AppKit" "-framework Foundation")
