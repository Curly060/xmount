#!/bin/bash

control_c() {
  echo
  echo Exiting
  exit 0
}

trap control_c SIGINT

if [[ "$1" != "debian" && "$1" != "osx" ]]; then
  echo "Usage: $0 <debian | osx> [<jobs>]"
  exit 1
fi

OS="$1"
JOBS=1

if [ ! -z "$2" ]; then
  JOBS=$2
fi

LINUX_LD_PATH=""
if [ "$OS" == "debian" ]; then
  # Need to find system library path
  ARCH=`uname -m`

  if [ "$ARCH" == "i686" ]; then
    LINUX_LD_PATH="/lib/i386-linux-gnu"
  elif [ "$ARCH" == "x86_64" ]; then
    LINUX_LD_PATH="/lib/x86_64-linux-gnu"
  else
    echo "This script only supports i686 and x86_64 architectures!"
    exit 1
  fi
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
# libaff4 requires libsnappy, libraptor2 & libxml2
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

# LibXML2
if [ "$OS" == "debian" ]; then
  # Under Linux, wee need to compile this ourself too...
  (
    cd libxmount_input/libxmount_input_aff4/
    echo
    echo "Extracting libxml2"
    rm -rf libxml2 &>/dev/null
    tar xfz libxml2-*.tar.gz
    echo
    read -p "Ready to configure LIBXML2?"
    cd libxml2
    CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./autogen.sh --enable-static=yes --enable-shared=yes --without-ftp --without-debug --without-icu --without-python
    echo
    read -p "Ready to compile LIBXML2?"
    make -j$JOBS
  )
fi

# LibRAPTOR2
(
  cd libxmount_input/libxmount_input_aff4/
  echo
  echo "Extracting libraptor2"
  rm -rf libraptor2 &>/dev/null
  tar xfz libraptor2-*.tar.gz
  echo
  read -p "Ready to configure LIBRAPTOR2?"
  cd libraptor2
  #CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --with-www=none --with-yajl=no --enable-release
  if [ "$OS" == "debian" ]; then
    CFLAGS="-fPIC -I$(pwd)/../libxml2/include" CXXFLAGS="-fPIC -I$(pwd)/../libxml2/include" LDFLAGS="-L$(pwd)/../libxml2/.libs" LIBXML_CFLAGS="-I$(pwd)/../libxml2/include" LIBXML_LIBS="-L$(pwd)/../libxml2/.libs -lxml2 -llzma -ldl -lz -lm" ./configure --with-www=none --with-yajl=no --enable-shared=yes --enable-static=yes --enable-release --with-xml2-config=$(pwd)/../libxml2/xml2-config
  else
    CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --enable-parsers=turtle --enable-serializers=ntriples --with-www=none --with-yajl=no --enable-release
  fi
  echo
  read -p "Ready to compile LIBRAPTOR2?"
  make -j$JOBS
  mkdir src/raptor2
  cp src/raptor2.h src/raptor2/
)

# LibAFF4
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
  #CFLAGS="-fPIC -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build" CXXFLAGS="-fPIC -std=c++11 -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build" LDFLAGS="-L$(pwd)/../libsnappy/build" RAPTOR2_LIBS="-L$(pwd)/../libraptor2/src/.libs -lraptor2" RAPTOR2_CFLAGS="-I$(pwd)/../libraptor2/src" ./configure --disable-shared --enable-static
  if [ "$OS" == "debian" ]; then
    CFLAGS="-fPIC -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build" CXXFLAGS="-fPIC -std=c++11 -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build" LDFLAGS="-L$(pwd)/../libsnappy/build" RAPTOR2_LIBS="-L$(pwd)/../libraptor2/src/.libs -lraptor2 -lxml2" RAPTOR2_CFLAGS="-I$(pwd)/../libraptor2/src" ./configure --disable-shared --enable-static
  else
    # On OSx, we need even more shenanigans for this to compile...
    CFLAGS="-fPIC -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build -DO_LARGEFILE=0x0 -Dpread64=pread" CXXFLAGS="-fPIC -I$(pwd)/../libsnappy -I$(pwd)/../libsnappy/build -DO_LARGEFILE=0x0 -Dpread64=pread -std=c++11" LDFLAGS="-L$(pwd)/../libsnappy/build" RAPTOR2_LIBS="-L$(pwd)/../libraptor2/src/.libs -lraptor2" RAPTOR2_CFLAGS="-I$(pwd)/../libraptor2/src" ./configure --disable-shared --enable-static
  fi
  echo
  read -p "Ready to compile LIBAFF4?"
  make -j$JOBS
)

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

