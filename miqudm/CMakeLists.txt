cmake_minimum_required(VERSION 3.20)
project(miqudm LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)
pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
pkg_check_modules(PAM REQUIRED IMPORTED_TARGET pam)
pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.20)
pkg_check_modules(LIBINPUT REQUIRED IMPORTED_TARGET libinput)

# Common Library
add_library(miqudm_common STATIC
    src/common/user_discovery.cpp
    src/common/session_discovery.cpp
    src/common/config.cpp
    src/common/ipc_protocol.cpp
)

target_include_directories(miqudm_common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
)

target_link_libraries(miqudm_common PUBLIC
    PkgConfig::MIQUTOOLKIT
    PkgConfig::JSONCPP
    stdc++fs
    pthread
)

target_compile_options(miqudm_common PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# Daemon Executable: miqudm
add_executable(miqudm
    src/daemon/main.cpp
    src/daemon/daemon.cpp
    src/daemon/pam_service.cpp
    src/daemon/logind_manager.cpp
    src/daemon/session_runner.cpp
)

target_include_directories(miqudm PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/daemon"
)

target_link_libraries(miqudm PRIVATE
    miqudm_common
    PkgConfig::PAM
    PkgConfig::SYSTEMD
    pthread
)

target_compile_options(miqudm PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# Greeter UI Executable: miqudm-greeter
add_executable(miqudm-greeter
    src/greeter/main.cpp
    src/greeter/greeter_app.cpp
    src/greeter/ipc_client.cpp
)

target_include_directories(miqudm-greeter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/greeter"
)

target_link_libraries(miqudm-greeter PRIVATE
    miqudm_common
    PkgConfig::MIQUTOOLKIT
    pthread
)

target_compile_options(miqudm-greeter PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# Kiosk Input Preload Hook (Enables tap-to-click & touch mapping in Cage/wlroots kiosks)
add_library(miqudm_kiosk_input SHARED
    src/greeter/kiosk_input_hook.c
)

target_link_libraries(miqudm_kiosk_input PRIVATE
    PkgConfig::WLROOTS
    PkgConfig::LIBINPUT
    ${CMAKE_DL_LIBS}
)

set_target_properties(miqudm_kiosk_input PROPERTIES
    OUTPUT_NAME "miqudm-kiosk-input"
    PREFIX "lib"
)

# Installation rules
include(GNUInstallDirs)

install(TARGETS miqudm miqudm-greeter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS miqudm_kiosk_input LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/miqudm)
install(FILES assets/miqudm.conf DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/miqudm)
install(FILES assets/miqudm.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqudm)
install(FILES assets/miqudm.pam DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/pam.d RENAME miqudm)
install(FILES assets/miqudm-greeter.pam DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/pam.d RENAME miqudm-greeter)
install(FILES assets/miqudm.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system OPTIONAL)
install(PROGRAMS assets/wayland-session DESTINATION ${CMAKE_INSTALL_LIBDIR}/miqudm)
