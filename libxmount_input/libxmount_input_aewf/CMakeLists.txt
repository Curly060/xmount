if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_aewf C)

if(CMAKE_THREAD_LIBS_INIT)
  set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(CMAKE_THREAD_LIBS_INIT)

add_library(xmount_input_aewf SHARED libxmount_input_aewf.c ../../libxmount/libxmount.c)

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(xmount_input_aewf PUBLIC "-pthread")
endif(THREADS_HAVE_PTHREAD_ARG)

if(NOT STATIC)
  include_directories(${LIBZ_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBZ_LIBRARIES})
else(NOT STATIC_EWF)
  include_directories(${LIBZ_INCLUDE_DIRS})
  if(NOT APPLE)
    set(LIBS ${LIBS} ${LIBZ_LIBRARIES})
  else(NOT APPLE)
    # As most of libewf's deps on OSx come from MacPorts, better link in
    # everything statically
    set(LIBS ${LIBS} "/opt/local/lib/libz.a")
  endif(NOT APPLE)
endif(NOT STATIC)

target_link_libraries(xmount_input_aewf ${LIBS})

install(TARGETS xmount_input_aewf DESTINATION lib/xmount)

