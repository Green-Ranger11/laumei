qt_add_library(qs_pipewire STATIC)
qt_add_qml_module(qs_pipewire
    URI Quickshell.Services.Pipewire
    VERSION 1.0
    SOURCES
        pipewire.mm pipewire.h
)
target_include_directories(qs_pipewire PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qs_pipewire PUBLIC Qt6::Core Qt6::Qml qs_io
    "-framework CoreAudio" "-framework AudioToolbox" "-framework Foundation")
