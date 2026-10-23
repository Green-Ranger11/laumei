cmake_minimum_required(VERSION 3.21)
project(quickshell-macos VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Network)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

# One static QML module per Quickshell import URI. Each is linked into the
# `qs` executable and pulled in with Q_IMPORT_QML_PLUGIN in main.cpp.
add_subdirectory(src/modules/io)
add_subdirectory(src/modules/wayland)
add_subdirectory(src/modules/core)
add_subdirectory(src/modules/widgets)

qt_add_executable(qs
    src/main.cpp
    src/ipc.cpp src/ipc.h
)

target_link_libraries(qs PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network
    qs_core qs_coreplugin
    qs_io qs_ioplugin
    qs_wayland qs_waylandplugin
    qs_widgets qs_widgetsplugin
)

# Linux tools that configs shell out to (wl-copy, notify-send, ...) mapped onto
# macOS equivalents. Process puts this directory first on PATH.
file(GLOB COMPAT_TOOLS ${CMAKE_SOURCE_DIR}/compat/bin/*)
foreach(tool ${COMPAT_TOOLS})
    get_filename_component(name ${tool} NAME)
    configure_file(${tool} ${CMAKE_BINARY_DIR}/share/quickshell-macos/compat/bin/${name} COPYONLY)
endforeach()

install(TARGETS qs RUNTIME DESTINATION bin)
install(DIRECTORY compat/bin/ DESTINATION share/quickshell-macos/compat/bin
        USE_SOURCE_PERMISSIONS)
