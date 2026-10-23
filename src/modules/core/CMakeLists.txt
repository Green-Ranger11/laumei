set_source_files_properties(Quickshell.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(qs_core STATIC)
qt_add_qml_module(qs_core
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
target_include_directories(qs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qs_core PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick qs_io qs_wayland
    "-framework AppKit" "-framework Foundation")
