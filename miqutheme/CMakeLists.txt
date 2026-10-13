cmake_minimum_required(VERSION 3.20)
project(miqutheme LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure RPATH so the executable always finds libmiqutoolkit.so
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)

add_executable(miqutheme
    src/main.cpp
    src/backend/theme_backend.cpp
    src/ui/color_scheme_view.cpp
    src/ui/wallpaper_view.cpp
)

target_include_directories(miqutheme PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(miqutheme PRIVATE
    PkgConfig::MIQUTOOLKIT
    Threads::Threads
    stdc++fs
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqutheme PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqutheme PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miqutheme BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqutheme BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqutheme PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqutheme RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/miqutheme.desktop")
    install(FILES assets/miqutheme.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()
install(FILES assets/miqutheme.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqutheme)
install(DIRECTORY assets/schemes DESTINATION ${CMAKE_INSTALL_DATADIR}/miqutheme)
install(DIRECTORY assets/wallpapers DESTINATION ${CMAKE_INSTALL_DATADIR}/miqutheme)
install(DIRECTORY assets/matugen DESTINATION ${CMAKE_INSTALL_DATADIR}/miqutheme)
install(FILES assets/default_dark_wallpaper.png assets/default_light_wallpaper.png DESTINATION ${CMAKE_INSTALL_DATADIR}/miqutheme)

install(FILES assets/miqutheme.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
install(FILES assets/miqutheme-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME miqutheme.png)
install(FILES assets/miqutheme-128.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps RENAME miqutheme.png)
install(FILES assets/miqutheme-64.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps RENAME miqutheme.png)
install(FILES assets/miqutheme-48.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps RENAME miqutheme.png)
install(FILES assets/miqutheme-32.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps RENAME miqutheme.png)
install(FILES assets/miqutheme-16.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps RENAME miqutheme.png)
