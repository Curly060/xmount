if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_aewf C)

if(CMAKE_THREAD_LIBS_INIT)
  set(LIB_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif(CMAKE_THREAD_LIBS_INIT)

set(LIB_LIBS ${LIB_LIBS} ${LIBZ_LIBRARIES})

add_library(xmount_input_aewf SHARED libxmount_input_aewf.c ../../libxmount/libxmount.c)

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(xmount_input_aewf PUBLIC "-pthread")
endif(THREADS_HAVE_PTHREAD_ARG)

include_directories(${LIBZ_INCLUDE_DIRS})

target_link_libraries(xmount_input_aewf ${LIB_LIBS})

install(TARGETS xmount_input_aewf DESTINATION lib/xmount)

