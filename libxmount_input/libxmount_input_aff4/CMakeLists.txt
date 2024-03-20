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
  set(LIBS ${LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/libraptor2/src/.libs/libraptor2.a)
  set(LIBS ${LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/libsnappy/build/libsnappy.a)

  if(NOT APPLE)
    set(LIBS ${LIBS} "-lstdc++ -lz -lm -ldl -llz4 -llzma -lxml2 -licui18n -licuuc -licudata")
  else(NOT APPLE)
    # As most of afflib4's deps on OSx come from MacPorts, better link in
    # everything statically
    set(LIBS ${LIBS} "/opt/local/lib/liblz4.a")
    set(LIBS ${LIBS} "/opt/local/lib/libz.a")
    set(LIBS ${LIBS} "-lstdc++")
  endif(NOT APPLE)
endif(NOT STATIC_AFF)

add_library(xmount_input_aff4 SHARED libxmount_input_aff4.c)

target_link_libraries(xmount_input_aff4 ${LIBS})

install(TARGETS xmount_input_aff4 DESTINATION lib/xmount)

