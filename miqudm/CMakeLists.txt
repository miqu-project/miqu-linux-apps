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

# Common Library
add_library(miqudm_common STATIC
    src/common/config.cpp
    src/common/ipc.cpp
)

target_include_directories(miqudm_common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
)

target_link_libraries(miqudm_common PUBLIC
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
    src/daemon/pam_auth.cpp
    src/daemon/session.cpp
    src/daemon/logind.cpp
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqudm-greeter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqudm-greeter BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqudm-greeter PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# Installation rules
include(GNUInstallDirs)

install(TARGETS miqudm miqudm-greeter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqudm.conf DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/miqudm)
install(FILES assets/miqudm.pam DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/pam.d RENAME miqudm)
install(FILES assets/miqudm-greeter.pam DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/pam.d RENAME miqudm-greeter)
install(FILES assets/miqudm.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system OPTIONAL)
install(PROGRAMS assets/wayland-session DESTINATION ${CMAKE_INSTALL_LIBDIR}/miqudm)
