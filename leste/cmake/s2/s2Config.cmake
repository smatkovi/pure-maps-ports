# Debian 12's s2geometry (libs2-dev 0.10) has no CMake package: its
# library as the target s2::s2 that Pure Maps links with
if(NOT TARGET s2::s2)
  find_library(S2_LIBRARY s2 REQUIRED)
  find_path(S2_INCLUDE_DIR s2/s2latlng.h REQUIRED)
  add_library(s2::s2 SHARED IMPORTED)
  set_target_properties(s2::s2 PROPERTIES
    IMPORTED_LOCATION "${S2_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${S2_INCLUDE_DIR}")
endif()
set(s2_FOUND TRUE)
