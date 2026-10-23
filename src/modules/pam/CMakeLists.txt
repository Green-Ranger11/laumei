qt_add_library(laumei_pam STATIC)
qt_add_qml_module(laumei_pam
    URI Quickshell.Services.Pam
    VERSION 1.0
    SOURCES
        pam.cpp pam.h
)
target_include_directories(laumei_pam PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_pam PUBLIC Qt6::Core Qt6::Qml)
