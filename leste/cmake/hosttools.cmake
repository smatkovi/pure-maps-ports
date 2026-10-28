# Qt 5's tools of the PC (same 5.15 series as the target's): the target's
# Qt5 CMake files keep tool targets that exist already
foreach(t moc rcc uic lrelease lupdate lconvert qmlcachegen)
  if(NOT TARGET Qt5::${t} AND EXISTS /usr/bin/${t}-qt5)
    add_executable(Qt5::${t} IMPORTED)
    set_target_properties(Qt5::${t} PROPERTIES IMPORTED_LOCATION /usr/bin/${t}-qt5)
  endif()
endforeach()
# what Qt5CoreConfigExtras.cmake sets only when it makes Qt5::moc itself
# (Pure Maps finds lconvert next to it)
set(QT_MOC_EXECUTABLE /usr/bin/moc-qt5)
