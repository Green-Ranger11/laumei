qt_add_library(laumei_io STATIC)
qt_add_qml_module(laumei_io
    URI Quickshell.Io
    VERSION 1.0
    SOURCES
        datastream.cpp datastream.h
        process.cpp process.h
        fileview.cpp fileview.h
        ipchandler.cpp ipchandler.h
        paths.cpp paths.h
        objectmodel.h
        procfs.mm procfs.h
)
target_include_directories(laumei_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(laumei_io PUBLIC Qt6::Core Qt6::Qml "-framework IOKit" "-framework Foundation")
