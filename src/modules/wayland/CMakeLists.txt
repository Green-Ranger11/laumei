set_source_files_properties(ToplevelManager.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(laumei_wayland STATIC)
qt_add_qml_module(laumei_wayland
    URI Quickshell.Wayland
    VERSION 1.0
    SOURCES
        layershell.cpp layershell.h
        session.mm session.h
    QML_FILES
        WlSessionLock.qml
        WlSessionLockSurface.qml
        ScreencopyView.qml
        ToplevelManager.qml
    DEPENDENCIES
        QtQuick
)
target_include_directories(laumei_wayland PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_wayland PUBLIC Qt6::Core Qt6::Qml laumei_io
    "-framework IOKit" "-framework ApplicationServices")
