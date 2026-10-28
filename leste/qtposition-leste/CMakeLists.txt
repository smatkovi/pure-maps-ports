cmake_minimum_required(VERSION 3.16)
project(qtposition-leste LANGUAGES CXX)

find_package(Qt5 5.14 REQUIRED COMPONENTS Core DBus Positioning)
find_package(PkgConfig REQUIRED)
# liblocation's headers name GLib's and libdbus'; Leste's liblocation.so is
# not linked to the libraries it uses, so they are linked here
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET gobject-2.0 dbus-1 dbus-glib-1 gconf-2.0)
find_path(LOCATION_INCLUDE_DIR location/location-gps-device.h REQUIRED)
find_library(LOCATION_LIBRARY location REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 11)

set(QT_INSTALL_PLUGINS "${CMAKE_INSTALL_PREFIX}/lib/qt5/plugins" CACHE PATH
    "where Qt's plugins are")

add_library(qtposition_leste MODULE plugin.cpp positionsource.cpp positionsource.h)
target_include_directories(qtposition_leste PRIVATE ${LOCATION_INCLUDE_DIR})
target_link_libraries(qtposition_leste Qt5::Core Qt5::DBus Qt5::Positioning
    ${LOCATION_LIBRARY} PkgConfig::GLIB)
target_link_options(qtposition_leste PRIVATE "LINKER:--no-as-needed")
install(TARGETS qtposition_leste DESTINATION "${QT_INSTALL_PLUGINS}/position")
