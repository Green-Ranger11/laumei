qt_add_library(laumei_networking STATIC)
qt_add_qml_module(laumei_networking
    URI Quickshell.Networking
    VERSION 1.0
    SOURCES
        networking.mm networking.h
)
target_include_directories(laumei_networking PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_networking PUBLIC Qt6::Core Qt6::Gui Qt6::Qml laumei_io
    "-framework CoreWLAN" "-framework CoreLocation" "-framework Foundation")
