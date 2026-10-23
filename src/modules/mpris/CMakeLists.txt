qt_add_library(laumei_mpris STATIC)
qt_add_qml_module(laumei_mpris
    URI Quickshell.Services.Mpris
    VERSION 1.0
    SOURCES
        mpris.mm mpris.h
)
target_include_directories(laumei_mpris PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_mpris PUBLIC Qt6::Core Qt6::Qml laumei_io "-framework AppKit")
