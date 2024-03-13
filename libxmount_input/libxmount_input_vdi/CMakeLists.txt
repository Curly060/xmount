if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_vdi C)

add_library(xmount_input_vdi SHARED libxmount_input_vdi.c ../../libxmount/libxmount.c)

install(TARGETS xmount_input_vdi DESTINATION lib/xmount)

