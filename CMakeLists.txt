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
add_subdirectory(src/modules/bluetooth)
add_subdirectory(src/modules/networking)

qt_add_executable(qs MACOSX_BUNDLE
    src/main.cpp
    src/ipc.cpp src/ipc.h
)

# A real .app so macOS can ask for Location (Wi-Fi network names) and
# Bluetooth access on its behalf. LSUIElement keeps it out of the Dock.
set_target_properties(qs PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.quickshell-macos
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)

target_link_libraries(qs PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network
    qs_core qs_coreplugin
    qs_io qs_ioplugin
    qs_wayland qs_waylandplugin
    qs_widgets qs_widgetsplugin
    qs_bluetooth qs_bluetoothplugin
    qs_networking qs_networkingplugin
)

# Linux tools that configs shell out to (wl-copy, notify-send, systemctl, ...)
# mapped onto macOS, plus their native helper. They live in the bundle's
# Resources; Process puts that directory first on PATH.
set(COMPAT_DIR $<TARGET_BUNDLE_CONTENT_DIR:qs>/Resources/compat/bin)

add_executable(qs-compat src/tools/qs-compat.mm)
target_link_libraries(qs-compat PRIVATE "-framework AppKit")
add_custom_command(TARGET qs-compat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COMPAT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:qs-compat> ${COMPAT_DIR}/)

file(GLOB COMPAT_TOOLS ${CMAKE_SOURCE_DIR}/compat/bin/*)
add_custom_target(compat-tools ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COMPAT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${COMPAT_TOOLS} ${COMPAT_DIR}/
    DEPENDS ${COMPAT_TOOLS} qs)
add_dependencies(qs-compat qs)

# <prefix>/Applications/qs.app, with <prefix>/bin/qs pointing into it.
install(TARGETS qs BUNDLE DESTINATION Applications)
install(CODE "
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/bin\")
    file(CREATE_LINK \"\${CMAKE_INSTALL_PREFIX}/Applications/qs.app/Contents/MacOS/qs\"
                     \"\${CMAKE_INSTALL_PREFIX}/bin/qs\" SYMBOLIC)
")
