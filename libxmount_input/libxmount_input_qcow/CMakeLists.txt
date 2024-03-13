if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_qcow C)

add_library(libxmount_input_qcow SHARED libxmount_input_qcow.c ../../libxmount/libxmount.c)

install(TARGETS libxmount_input_qcow DESTINATION lib/xmount)

