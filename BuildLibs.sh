#!/bin/bash

control_c() {
  echo
  echo Exiting
  exit 0
}

trap control_c SIGINT

if [[ "$1" != "linux" && "$1" != "osx" ]]; then
  echo "Usage: $0 <linux | osx> [<jobs>]"
  exit 1
fi

PLATFORM="$1"
JOBS=1

if [ ! -z "$2" ]; then
  JOBS=$2
fi

############################## LibAFF
(
  cd libxmount_input/libxmount_input_aff/
  echo
  echo "Extracting libaff"
  rm -rf libaff &>/dev/null
  tar xfz libaff-*.tar.gz
  echo
  read -p "Ready to configure LIBAFF?"
  cd libaff
  ./bootstrap.sh
  CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --disable-qemu --disable-fuse --disable-s3 --disable-shared --enable-static --enable-threading
  echo
  read -p "Ready to compile LIBAFF?"
  make -j$JOBS
)

############################## LibAFF4
# libaff4 requires libsnappy & libraptor2
# LibSNAPPY
(
  cd libxmount_input/libxmount_input_aff4/
  echo
  echo "Extracting libsnappy"
  rm -rf libsnappy &>/dev/null
  tar xfz libsnappy-*.tar.gz
  echo
  read -p "Ready to configure LIBSNAPPY?"
  mkdir libsnappy/build
  cd libsnappy/build
  CFLAGS="-fPIC" CXXFLAGS="-fPIC" cmake .. -DBUILD_SHARED_LIBS=ON -DSNAPPY_BUILD_BENCHMARKS=OFF -DSNAPPY_BUILD_TESTS=OFF -DSNAPPY_INSTALL=OFF
  echo
  read -p "Ready to compile LIBSNAPPY?"
  make -j$JOBS
)

# LibRAPTOR2
if [ "$PLATFORM" == "linux" ]; then
  (
    cd libxmount_input/libxmount_input_aff4/
    echo
    echo "Extracting libraptor2"
    rm -rf libraptor2 &>/dev/null
    tar xfz libraptor2-*.tar.gz
    echo
    read -p "Ready to configure LIBRAPTOR2?"
    cd libraptor2
    CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --with-www=none --with-yajl=no --enable-release
    echo
    read -p "Ready to compile LIBRAPTOR2?"
    make -j$JOBS
    mkdir src/raptor2
    cp src/raptor2.h src/raptor2/
  )
else
  (
    cd libxmount_input/libxmount_input_aff4/
    echo
    echo "Extracting libraptor2"
    rm -rf libraptor2 &>/dev/null
    tar xfz libraptor2-*.tar.gz
    echo
    read -p "Ready to configure LIBRAPTOR2?"
    cd libraptor2
    CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --enable-parsers=turtle --enable-serializers=ntriples --with-www=none --with-yajl=no --enable-release
    echo
    read -p "Ready to compile LIBRAPTOR2?"
    make -j$JOBS
    mkdir src/raptor2
    cp src/raptor2.h src/raptor2/
  )
fi

# LibAFF4
if [ "$PLATFORM" == "linux" ]; then
  (
    cd libxmount_input/libxmount_input_aff4/
    echo
    echo "Extracting libaff4"
    rm -rf libaff4 &>/dev/null
    tar xfz libaff4-*.tar.gz
    echo
    read -p "Ready to configure LIBAFF4?"
    cd libaff4
    ./autogen.sh
    CFLAGS="-fPIC -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build" CXXFLAGS="-fPIC -std=c++11 -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build" LDFLAGS="-L$(pwd)/../libsnappy/build" RAPTOR2_LIBS="-L$(pwd)/../libraptor2/src/.libs -lraptor2" RAPTOR2_CFLAGS="-I$(pwd)/../libraptor2/src" ./configure --disable-shared --enable-static
    echo
    read -p "Ready to compile LIBAFF4?"
    make -j$JOBS
  )
else
  # On OSx, we need some more shenanigans for this to compile...
  (
    cd libxmount_input/libxmount_input_aff4/
    echo
    echo "Extracting libaff4"
    rm -rf libaff4 &>/dev/null
    tar xfz libaff4-*.tar.gz
    echo
    read -p "Ready to configure LIBAFF4?"
    cd libaff4
    ./autogen.sh
    CFLAGS="-fPIC -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build -DO_LARGEFILE=0x0 -Dpread64=pread" CXXFLAGS="-fPIC -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build -DO_LARGEFILE=0x0 -Dpread64=pread -std=c++11" LDFLAGS="-L$(pwd)/../libsnappy/build" RAPTOR2_LIBS="-L$(pwd)/../libraptor2/src/.libs -lraptor2" RAPTOR2_CFLAGS="-I$(pwd)/../libraptor2/src" ./configure --disable-shared --enable-static
    echo
    read -p "Ready to compile LIBAFF4?"
    make -j$JOBS
  )
fi

############################## LibEWF
(
  cd libxmount_input/libxmount_input_ewf
  echo
  echo "Extracting libewf"
  rm -rf libewf &>/dev/null
  tar xfz libewf-*.tar.gz
  echo
  read -p "Ready to configure LIBEWF?"
  cd libewf
  CFLAGS="-fPIC" ./configure --disable-v1-api --disable-shared --enable-static --without-libbfio --without-libfuse --without-openssl
  echo
  read -p "Ready to compile LIBEWF?"
  make -j$JOBS
)

