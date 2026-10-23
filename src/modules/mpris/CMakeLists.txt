qt_add_library(qs_mpris STATIC)
qt_add_qml_module(qs_mpris
    URI Quickshell.Services.Mpris
    VERSION 1.0
    SOURCES
        mpris.mm mpris.h
)
target_include_directories(qs_mpris PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qs_mpris PUBLIC Qt6::Core Qt6::Qml qs_io "-framework AppKit")
