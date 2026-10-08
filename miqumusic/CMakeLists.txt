cmake_minimum_required(VERSION 3.20)
project(miqumusic LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)
pkg_check_modules(LIBMPDCLIENT REQUIRED IMPORTED_TARGET libmpdclient)

add_executable(miqumusic
    src/main.cpp
    src/music_app.cpp
    src/mpd_client.cpp
    src/art_loader.cpp
    src/ui/player_view.cpp
    src/ui/queue_view.cpp
    src/ui/library_view.cpp
    src/ui/artists_view.cpp
    src/ui/settings_view.cpp
)

target_include_directories(miqumusic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(miqumusic PRIVATE
    PkgConfig::MIQUTOOLKIT
    PkgConfig::LIBMPDCLIENT
    Threads::Threads
    stdc++fs
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqumusic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqumusic BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqumusic PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqumusic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqumusic.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/miqumusic.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES assets/miqumusic.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqumusic)
