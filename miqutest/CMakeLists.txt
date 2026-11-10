cmake_minimum_required(VERSION 3.20)
project(miqutest LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)
pkg_check_modules(SDBUSCPP REQUIRED IMPORTED_TARGET sdbus-c++)

add_executable(miqutest
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
    src/ethernet_backend.cpp
    src/firewall_backend.cpp
    src/airplane_backend.cpp
)

target_include_directories(miqutest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(miqutest PRIVATE
    PkgConfig::MIQUTOOLKIT
    PkgConfig::SDBUSCPP
    Threads::Threads
    stdc++fs
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqutest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqutest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miqutest BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqutest BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqutest PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqutest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqutest.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqutest)
install(FILES assets/miqutest.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/miquland.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES assets/miquland-128.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps RENAME miquland.png)
install(FILES assets/miquland-64.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps RENAME miquland.png)
install(FILES assets/miquland-48.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps RENAME miquland.png)
