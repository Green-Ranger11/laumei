qt_add_library(laumei_widgets STATIC)
qt_add_qml_module(laumei_widgets
    URI Quickshell.Widgets
    VERSION 1.0
    QML_FILES
        IconImage.qml
    DEPENDENCIES
        QtQuick
)
