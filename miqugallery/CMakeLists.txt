cmake_minimum_required(VERSION 3.20)
project(miqugallery LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)

add_executable(miqugallery
    src/main.cpp
    src/media_scanner.cpp
    src/media_viewer.cpp
)

target_include_directories(miqugallery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(miqugallery PRIVATE
    PkgConfig::MIQUTOOLKIT
    Threads::Threads
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqugallery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqugallery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miqugallery BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqugallery BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqugallery PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqugallery RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/miqugallery.desktop")
    install(FILES assets/miqugallery.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()
install(FILES assets/miqugallery.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqugallery)
install(FILES assets/miqugallery.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
install(FILES assets/miqugallery-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME miqugallery.png)
install(FILES assets/miqugallery-128.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps RENAME miqugallery.png)
install(FILES assets/miqugallery-64.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps RENAME miqugallery.png)
install(FILES assets/miqugallery-48.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps RENAME miqugallery.png)
install(FILES assets/miqugallery-32.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps RENAME miqugallery.png)
install(FILES assets/miqugallery-16.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps RENAME miqugallery.png)
