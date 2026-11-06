cmake_minimum_required(VERSION 3.20)
project(miqusettings LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)
pkg_check_modules(SDBUSCPP REQUIRED IMPORTED_TARGET sdbus-c++)

add_executable(miqusettings
    src/main.cpp
    src/config/settings_config.cpp
    src/model/setting_item.cpp
    src/ui/common/modal_overlay.cpp
    src/modules/bluetooth/bluetooth_backend.cpp
    src/modules/bluetooth/bluetooth_agent.cpp
    src/modules/bluetooth/bluetooth_detail_view.cpp
    src/modules/ethernet/ethernet_backend.cpp
    src/modules/ethernet/ethernet_detail_view.cpp
    src/modules/wifi/wifi_backend.cpp
    src/modules/wifi/wifi_detail_view.cpp
    src/modules/airplane/airplane_backend.cpp
    src/modules/airplane/airplane_detail_view.cpp
    src/modules/applications/apps_backend.cpp
    src/modules/applications/app_detail_view.cpp
    src/modules/applications/default_app_detail_view.cpp
    src/ui/setting_detail_view.cpp
    src/ui/settings_window.cpp
)

target_include_directories(miqusettings PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(miqusettings PRIVATE
    PkgConfig::MIQUTOOLKIT
    PkgConfig::SDBUSCPP
    stdc++fs
    pthread
)

# Local in-tree dev tree fallback
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqusettings PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqusettings PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miqusettings BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqusettings BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqusettings PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqusettings RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqusettings.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqusettings)
install(FILES assets/miqusettings.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
