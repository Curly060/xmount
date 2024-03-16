if(NOT APPLE)
  include_directories(${LIBFUSE_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBFUSE_LIBRARIES})
else(NOT APPLE)
  include_directories(${LIBOSXFUSE_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBOSXFUSE_LIBRARIES})
  link_directories(${LIBOSXFUSE_LIBRARY_DIRS})
endif(NOT APPLE)

if(CMAKE_THREAD_LIBS_INIT)
  set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(CMAKE_THREAD_LIBS_INIT)

set(LIBS ${LIBS} "dl")

add_definitions(-DXMOUNT_LIBRARY_PATH="${CMAKE_INSTALL_PREFIX}/lib/xmount")

add_executable(xmount xmount.c md5.c ../libxmount/libxmount.c)

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(xmount PUBLIC "-pthread")
endif(THREADS_HAVE_PTHREAD_ARG)

target_link_libraries(xmount ${LIBS})

install(TARGETS xmount DESTINATION bin)

