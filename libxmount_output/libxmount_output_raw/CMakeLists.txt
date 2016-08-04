if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_output_raw C)

add_library(xmount_output_raw SHARED libxmount_output_raw.c ../../libxmount/libxmount.c)

install(TARGETS xmount_output_raw DESTINATION lib/xmount)

