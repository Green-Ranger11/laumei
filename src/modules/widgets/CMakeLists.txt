qt_add_library(qs_widgets STATIC)
qt_add_qml_module(qs_widgets
    URI Quickshell.Widgets
    VERSION 1.0
    QML_FILES
        IconImage.qml
    DEPENDENCIES
        QtQuick
)
