qt_add_library(qs_networking STATIC)
qt_add_qml_module(qs_networking
    URI Quickshell.Networking
    VERSION 1.0
    SOURCES
        networking.mm networking.h
)
target_include_directories(qs_networking PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qs_networking PUBLIC Qt6::Core Qt6::Gui Qt6::Qml qs_io
    "-framework CoreWLAN" "-framework CoreLocation" "-framework Foundation")
