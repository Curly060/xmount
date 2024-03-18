#!/bin/bash

# Create OSx package using Packages (http://s.sudre.free.fr/Software/Packages/about.html)

if [ "$(whoami)" != "root" ]; then
  echo "ERROR: This script has to be run as root!"
  exit 1
fi

CWD=`dirname "$0"`
CWD=`cd "$CWD"; pwd`
DSTROOT="$CWD/xmount_pkg/dstroot"
FULL_PKG_NAME=`basename "$CWD"`
PKG_VERSION=`echo "$FULL_PKG_NAME" | cut -d"-" -f2`

echo "==== PKG build settings ==="
echo "\$CWD=\"$CWD\""
echo "\$DSTROOT=\"$DSTROOT\""
echo "\$PKG_VERSION=\"$PKG_VERSION\""
echo
echo "Press any key to continue or Ctrl-C to cancel"
read

# Create new dstroot folder
rm -rf "$DSTROOT" &>/dev/null
mkdir -p "$DSTROOT"/usr/local/bin
mkdir -p "$DSTROOT"/usr/local/lib/xmount
mkdir -p "$DSTROOT"/usr/local/share/man/man1

# Populate dstroot with files
cp "$CWD"/build/src/xmount "$DSTROOT"/usr/local/bin/
find "$CWD"/build/libxmount_input -name "libxmount_input_*.dylib" -exec cp "{}" "$DSTROOT"/usr/local/lib/xmount/ \;
find "$CWD"/build/libxmount_morphing -name "libxmount_morphing_*.dylib" -exec cp "{}" "$DSTROOT"/usr/local/lib/xmount/ \;
cp "$CWD"/xmount.1 "$DSTROOT"/usr/local/share/man/man1/

# Patch project file
sed -i -e "s#%%XMOUNT_VERSION%%#$PKG_VERSION#g" "$CWD"/xmount_pkg/xmount.pkgproj

#open "$CWD"/xmount_pkg/xmount.pkgproj
packagesbuild "$CWD/xmount_pkg/xmount.pkgproj"

