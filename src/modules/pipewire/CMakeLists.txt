qt_add_library(laumei_pipewire STATIC)
qt_add_qml_module(laumei_pipewire
    URI Quickshell.Services.Pipewire
    VERSION 1.0
    SOURCES
        pipewire.mm pipewire.h
)
target_include_directories(laumei_pipewire PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_pipewire PUBLIC Qt6::Core Qt6::Qml laumei_io
    "-framework CoreAudio" "-framework AudioToolbox" "-framework Foundation")
