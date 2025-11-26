if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_morphing_byteswap C)

add_library(xmount_morphing_byteswap SHARED libxmount_morphing_byteswap.c ../../libxmount/libxmount.c)

install(TARGETS xmount_morphing_byteswap DESTINATION lib/xmount)

