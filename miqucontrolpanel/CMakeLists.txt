cmake_minimum_required(VERSION 3.20)
project(miqucontrolpanel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)
pkg_check_modules(SDBUSCPP REQUIRED IMPORTED_TARGET sdbus-c++)

add_executable(miqucontrolpanel
    src/main.cpp
    src/settings_model.cpp
    src/miller_view.cpp
    src/preview_form.cpp
    src/preview_network.cpp
    src/preview_personalization.cpp
    src/preview_hardware.cpp
    src/preview_apps.cpp
    src/test_app.cpp
    src/wifi_backend.cpp
    src/bluetooth_backend.cpp
    src/bluetooth_agent.cpp
    src/display_backend.cpp
    src/monitor_backend.cpp
    src/ethernet_backend.cpp
    src/firewall_backend.cpp
    src/airplane_backend.cpp
    src/dns_backend.cpp
    src/vpn_backend.cpp
    src/apps_backend.cpp
    src/sound_backend.cpp
    src/xdg_backend.cpp
    src/battery_backend.cpp
    src/input_backend.cpp
    src/preview_input.cpp
)

target_include_directories(miqucontrolpanel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(miqucontrolpanel PRIVATE
    PkgConfig::MIQUTOOLKIT
    PkgConfig::SDBUSCPP
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    stdc++fs
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqucontrolpanel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqucontrolpanel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miqucontrolpanel BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqucontrolpanel BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqucontrolpanel PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqucontrolpanel RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqucontrolpanel.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqucontrolpanel)
install(FILES assets/miqucontrolpanel.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/miquland.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES assets/miquland-128.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps RENAME miquland.png)
install(FILES assets/miquland-64.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps RENAME miquland.png)
install(FILES assets/miquland-48.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps RENAME miquland.png)
