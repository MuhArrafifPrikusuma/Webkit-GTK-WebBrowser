cmake_minimum_required(VERSION 3.16)
project(WebKitBrowser C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkitgtk-6.0)

set(SOURCES
    main.c
    tabs/tabs.c
    webview/webview.c
    sidebar/sidebar.c
    toolbar/toolbar.c
    search/search.c
    memory/memory.c
    spotlight/spotlight.c
    css/css.c
)

add_executable(webkit ${SOURCES})

target_link_libraries(webkit
    PRIVATE
        PkgConfig::GTK4
        PkgConfig::WEBKIT
        -lm
)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    message(STATUS "Link-Time Optimization (LTO) is supported and enabled.")
    set_property(TARGET webkit PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "LTO not supported: ${error}")
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/css DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

