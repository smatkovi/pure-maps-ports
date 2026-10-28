cmake_minimum_required(VERSION 3.16)
project(qtposition-leste LANGUAGES CXX)

find_package(Qt5 5.14 REQUIRED COMPONENTS Core DBus Positioning)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 11)

set(QT_INSTALL_PLUGINS "${CMAKE_INSTALL_PREFIX}/lib/qt5/plugins" CACHE PATH
    "where Qt's plugins are")

add_library(qtposition_leste MODULE plugin.cpp positionsource.cpp positionsource.h)
target_link_libraries(qtposition_leste Qt5::Core Qt5::DBus Qt5::Positioning)
install(TARGETS qtposition_leste DESTINATION "${QT_INSTALL_PLUGINS}/position")
