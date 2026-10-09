cmake_minimum_required(VERSION 3.20)
project(miqusecure LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure RPATH so the executable always finds libmiqutoolkit.so
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)

add_executable(miqusecure
    src/main.cpp
    src/backend/security_backend.cpp
    src/ui/firewall_view.cpp
    src/ui/traffic_view.cpp
    src/ui/dns_view.cpp
    src/ui/metadata_view.cpp
    src/ui/hardware_view.cpp
    src/ui/sandbox_view.cpp
    src/ui/testbed_view.cpp
)

target_include_directories(miqusecure PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(miqusecure PRIVATE
    PkgConfig::MIQUTOOLKIT
    Threads::Threads
    stdc++fs
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqusecure PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqusecure BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqusecure PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqusecure RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/miqusecure.desktop")
    install(FILES assets/miqusecure.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()
install(FILES assets/miqusecure.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqusecure)
install(FILES assets/miqusecure.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
install(FILES assets/miqusecure-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME miqusecure.png)
install(FILES assets/miqusecure-128.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps RENAME miqusecure.png)
install(FILES assets/miqusecure-64.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps RENAME miqusecure.png)
install(FILES assets/miqusecure-48.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps RENAME miqusecure.png)
install(FILES assets/miqusecure-32.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps RENAME miqusecure.png)
install(FILES assets/miqusecure-16.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps RENAME miqusecure.png)
