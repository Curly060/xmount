# Try pkg-config first
find_package(PkgConfig)
pkg_check_modules(PKGC_LIBOSXFUSE QUIET osxfuse)

if(PKGC_LIBOSXFUSE_FOUND)
  # Found lib using pkg-config.
  if(CMAKE_DEBUG)
    message(STATUS "\${PKGC_LIBOSXFUSE_LIBRARIES} = ${PKGC_LIBOSXFUSE_LIBRARIES}")
    message(STATUS "\${PKGC_LIBOSXFUSE_LIBRARY_DIRS} = ${PKGC_LIBOSXFUSE_LIBRARY_DIRS}")
    message(STATUS "\${PKGC_LIBOSXFUSE_LDFLAGS} = ${PKGC_LIBOSXFUSE_LDFLAGS}")
    message(STATUS "\${PKGC_LIBOSXFUSE_LDFLAGS_OTHER} = ${PKGC_LIBOSXFUSE_LDFLAGS_OTHER}")
    message(STATUS "\${PKGC_LIBOSXFUSE_INCLUDE_DIRS} = ${PKGC_LIBOSXFUSE_INCLUDE_DIRS}")
    message(STATUS "\${PKGC_LIBOSXFUSE_CFLAGS} = ${PKGC_LIBOSXFUSE_CFLAGS}")
    message(STATUS "\${PKGC_LIBOSXFUSE_CFLAGS_OTHER} = ${PKGC_LIBOSXFUSE_CFLAGS_OTHER}")
  endif(CMAKE_DEBUG)

  set(LIBOSXFUSE_LIBRARIES ${PKGC_LIBOSXFUSE_LIBRARIES})
  set(LIBOSXFUSE_INCLUDE_DIRS ${PKGC_LIBOSXFUSE_INCLUDE_DIRS})
  #set(LIBOSXFUSE_DEFINITIONS ${PKGC_LIBOSXFUSE_CFLAGS_OTHER})
else(PKGC_LIBOSXFUSE_FOUND)
  # Didn't find lib using pkg-config. Try to find it manually
  message(WARNING "Unable to find LibOSXFUSE using pkg-config! If compilation fails, make sure pkg-config is installed and PKG_CONFIG_PATH is set correctly")

  find_path(LIBOSXFUSE_INCLUDE_DIR fuse.h
            PATH_SUFFIXES fuse)
  find_library(LIBOSXFUSE_LIBRARY NAMES osxfuse libosxfuse)

  if(CMAKE_DEBUG)
    message(STATUS "\${LIBOSXFUSE_LIBRARY} = ${LIBOSXFUSE_LIBRARY}")
    message(STATUS "\${LIBOSXFUSE_INCLUDE_DIR} = ${LIBOSXFUSE_INCLUDE_DIR}")
  endif(CMAKE_DEBUG)

  set(LIBOSXFUSE_LIBRARIES ${LIBOSXFUSE_LIBRARY})
  set(LIBOSXFUSE_INCLUDE_DIRS ${LIBOSXFUSE_INCLUDE_DIR})
endif(PKGC_LIBOSXFUSE_FOUND)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set <PREFIX>_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(LibOSXFUSE DEFAULT_MSG LIBOSXFUSE_LIBRARIES)

