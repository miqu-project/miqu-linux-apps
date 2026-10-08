cmake_minimum_required(VERSION 3.20)
project(miqulauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure RPATH so the executable always finds libmiqutoolkit.so
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)

add_executable(miqulauncher
    src/main.cpp
    src/system/package_manager.cpp
    src/system/binary_manager.cpp
    src/system/cli_parser.cpp
    src/system/config_manager.cpp
    src/providers/app_provider.cpp
    src/providers/window_provider.cpp
    src/providers/workspace_provider.cpp
    src/providers/run_provider.cpp
    src/providers/script_provider.cpp
    src/providers/dmenu_provider.cpp
    src/ui/grid_item_view.cpp
    src/ui/launcher_window.cpp
)

target_include_directories(miqulauncher PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(miqulauncher PRIVATE
    PkgConfig::MIQUTOOLKIT
    stdc++fs
    pthread
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqulauncher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqulauncher BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqulauncher PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqulauncher RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqulauncher.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqulauncher)
