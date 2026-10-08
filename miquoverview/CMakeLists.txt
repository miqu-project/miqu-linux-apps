cmake_minimum_required(VERSION 3.20)
project(miquoverview VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.20)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(PANGO REQUIRED IMPORTED_TARGET pango pangocairo)

# Look for miquland pkg-config, or use local source tree fallback
pkg_check_modules(MIQULAND miquland)

set(SOURCES
    src/cairo_buffer.cpp
    src/overview_card.cpp
    src/overview_state.cpp
    src/overview_plugin.cpp
)

add_library(miquoverview SHARED ${SOURCES})

target_include_directories(miquoverview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../miquland/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../miquland/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../miquland/subprojects/scenefx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../miquland/build
)

if (MIQULAND_FOUND)
    target_include_directories(miquoverview PRIVATE ${MIQULAND_INCLUDE_DIRS})
endif()

target_link_libraries(miquoverview PRIVATE
    PkgConfig::WLROOTS
    PkgConfig::WAYLAND_SERVER
    PkgConfig::XKBCOMMON
    PkgConfig::PIXMAN
    PkgConfig::CAIRO
    PkgConfig::PANGO
)

target_compile_options(miquoverview PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miquoverview LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/miquland/plugins)
