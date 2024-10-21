# Try pkg-config first
find_package(PkgConfig)
pkg_check_modules(PKGC_LIBAFF4 QUIET libaff4)

if(PKGC_LIBAFF4_FOUND)
  # Found lib using pkg-config.
  if(CMAKE_DEBUG)
    message(STATUS "\${PKGC_LIBAFF4_LIBRARIES} = ${PKGC_LIBAFF4_LIBRARIES}")
    message(STATUS "\${PKGC_LIBAFF4_LIBRARY_DIRS} = ${PKGC_LIBAFF4_LIBRARY_DIRS}")
    message(STATUS "\${PKGC_LIBAFF4_LDFLAGS} = ${PKGC_LIBAFF4_LDFLAGS}")
    message(STATUS "\${PKGC_LIBAFF4_LDFLAGS_OTHER} = ${PKGC_LIBAFF4_LDFLAGS_OTHER}")
    message(STATUS "\${PKGC_LIBAFF4_INCLUDE_DIRS} = ${PKGC_LIBAFF4_INCLUDE_DIRS}")
    message(STATUS "\${PKGC_LIBAFF4_CFLAGS} = ${PKGC_LIBAFF4_CFLAGS}")
    message(STATUS "\${PKGC_LIBAFF4_CFLAGS_OTHER} = ${PKGC_LIBAFF4_CFLAGS_OTHER}")
  endif(CMAKE_DEBUG)

  set(LIBAFF4_LDFLAGS ${PKGC_LIBAFF4_LDFLAGS})
  set(LIBAFF4_LIBRARIES ${PKGC_LIBAFF4_LIBRARIES})
  set(LIBAFF4_INCLUDE_DIRS ${PKGC_LIBAFF4_INCLUDE_DIRS})
else(PKGC_LIBAFF4_FOUND)
  # Didn't find lib using pkg-config. Try to find it manually
  find_path(LIBAFF4_INCLUDE_DIR aff4-c.h PATH_SUFFIXES aff4)
  find_library(LIBAFF4_LIBRARY NAMES aff4 libaff4)

  if(CMAKE_DEBUG)
    message(STATUS "\${LIBAFF4_LIBRARY} = ${LIBAFF4_LIBRARY}")
    message(STATUS "\${LIBAFF4_INCLUDE_DIR} = ${LIBAFF4_INCLUDE_DIR}")
  endif(CMAKE_DEBUG)

  set(LIBAFF4_LIBRARIES ${LIBAFF4_LIBRARY})
  if(LIBAFF4_LIBRARIES)
    set(LIBAFF4_LIBRARIES ${LIBAFF4_LIBRARIES} "-lz -llz4 -lraptor2 -lsnappy")
  endif(LIBAFF4_LIBRARIES)
  set(LIBAFF4_INCLUDE_DIRS ${LIBAFF4_INCLUDE_DIR})
endif(PKGC_LIBAFF4_FOUND)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set <PREFIX>_FOUND to TRUE if all listed variables are TRUE
find_package_handle_standard_args(LibAFF4 DEFAULT_MSG LIBAFF4_INCLUDE_DIRS LIBAFF4_LIBRARIES)

if(NOT LIBAFF4_FOUND)
  message(STATUS "Unable to find AFFv4 library, disabling AFFv4 input support!")
endif(NOT LIBAFF4_FOUND)

