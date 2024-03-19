if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_aff4 C)

if(NOT STATIC_AFF)
  include_directories(${LIBAFF4_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBAFF4_LIBRARIES})
else(NOT STATIC_AFF)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libaff4/src)
  set(LIBS ${LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/libaff4/src/.libs/libaff4.a)
  if(NOT APPLE)
    #if(CMAKE_THREAD_LIBS_INIT)
    #  set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
    #endif(CMAKE_THREAD_LIBS_INIT)

    #set(LIBS ${LIBS} "-lstdc++ -lz -ldl")
    set(LIBS ${LIBS} "-lstdc++ -lz -llz4 -lraptor2 -lsnappy")

    #if(THREADS_HAVE_PTHREAD_ARG)
    #  target_compile_options(xmount_input_aff4 PUBLIC "-pthread")
    #endif(THREADS_HAVE_PTHREAD_ARG)
  else(NOT APPLE)
    # As most of afflib4's deps on OSx come from MacPorts, better link in
    # everything statically
    #set(LIBS ${LIBS} "/opt/local/lib/libcrypto.a")
    #set(LIBS ${LIBS} "/opt/local/lib/libssl.a")
    #set(LIBS ${LIBS} "/opt/local/lib/libexpat.a")
    #set(LIBS ${LIBS} "/opt/local/lib/libncurses.a")
    #set(LIBS ${LIBS} "/opt/local/lib/libreadline.a")
    set(LIBS ${LIBS} "/opt/local/lib/libz.a")
    set(LIBS ${LIBS} "-lstdc++")
  endif(NOT APPLE)
endif(NOT STATIC_AFF)

add_library(xmount_input_aff4 SHARED libxmount_input_aff4.c)

target_link_libraries(xmount_input_aff4 ${LIBS})

install(TARGETS xmount_input_aff4 DESTINATION lib/xmount)
