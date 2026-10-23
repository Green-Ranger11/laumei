cmake_minimum_required(VERSION 3.21)
project(laumei VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Network)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

# One static QML module per Quickshell import URI. Each is linked into the
# `laumei` executable and pulled in with Q_IMPORT_QML_PLUGIN in main.cpp.
add_subdirectory(src/modules/io)
add_subdirectory(src/modules/wayland)
add_subdirectory(src/modules/core)
add_subdirectory(src/modules/widgets)
add_subdirectory(src/modules/bluetooth)
add_subdirectory(src/modules/networking)
add_subdirectory(src/modules/hyprland)
add_subdirectory(src/modules/pipewire)
add_subdirectory(src/modules/upower)
add_subdirectory(src/modules/mpris)

qt_add_executable(laumei MACOSX_BUNDLE
    src/main.cpp
    src/ipc.cpp src/ipc.h
)

# A real .app so macOS can ask for Location (Wi-Fi network names) and
# Bluetooth access on its behalf. LSUIElement keeps it out of the Dock.
set_target_properties(laumei PROPERTIES
    OUTPUT_NAME Laumei
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.laumei
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)

target_link_libraries(laumei PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network
    laumei_core laumei_coreplugin
    laumei_io laumei_ioplugin
    laumei_wayland laumei_waylandplugin
    laumei_widgets laumei_widgetsplugin
    laumei_bluetooth laumei_bluetoothplugin
    laumei_networking laumei_networkingplugin
    laumei_hyprland laumei_hyprlandplugin
    laumei_pipewire laumei_pipewireplugin
    laumei_upower laumei_upowerplugin
    laumei_mpris laumei_mprisplugin
)

# Linux tools that configs shell out to (wl-copy, notify-send, systemctl, ...)
# mapped onto macOS, plus their native helper. They live in the bundle's
# Resources; Process puts that directory first on PATH.
set(COMPAT_DIR $<TARGET_BUNDLE_CONTENT_DIR:laumei>/Resources/compat/bin)

add_executable(laumei-compat src/tools/laumei-compat.mm)
target_link_libraries(laumei-compat PRIVATE "-framework AppKit")
add_custom_command(TARGET laumei-compat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COMPAT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:laumei-compat> ${COMPAT_DIR}/)

file(GLOB COMPAT_TOOLS ${CMAKE_SOURCE_DIR}/compat/bin/*)
add_custom_target(compat-tools ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COMPAT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${COMPAT_TOOLS} ${COMPAT_DIR}/
    DEPENDS ${COMPAT_TOOLS} laumei)
add_dependencies(laumei-compat laumei)

# <prefix>/Applications/Laumei.app, with <prefix>/bin/laumei pointing into it.
install(TARGETS laumei BUNDLE DESTINATION Applications)
install(CODE "
    set(exe \"\${CMAKE_INSTALL_PREFIX}/Applications/Laumei.app/Contents/MacOS/Laumei\")
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/bin\")
    file(CREATE_LINK \"\${exe}\" \"\${CMAKE_INSTALL_PREFIX}/bin/laumei\" SYMBOLIC)
")
