#!/usr/bin/env bash
# Builds devkitA64 (binutils 2.46, gcc 16.1.0, devkitPro newlib) from GitHub sources,
# following devkitPro/buildscripts, into /opt/devkitpro. Used when pkg.devkitpro.org is unreachable.
set -euo pipefail
J=${J:-2}
SRC=$HOME/tc/src; BLD=$HOME/tc/build; BS=$HOME/ref/buildscripts
export DEVKITPRO=/opt/devkitpro
prefix=$DEVKITPRO/devkitA64; target=aarch64-none-elf
export PATH=$prefix/bin:$PATH
mkdir -p $BLD; sudo mkdir -p $DEVKITPRO; sudo chown -R $(id -u):$(id -g) $DEVKITPRO

step() { echo "=== $(date +%T) $*"; }

# patches
if [ ! -f $BLD/patched ]; then
  (cd $SRC/binutils && git apply --exclude='*/ChangeLog*' $BS/patches/binutils-2.46.0-1.patch || patch -p1 -N < $BS/patches/binutils-2.46.0-1.patch) || true
  (cd $SRC/gcc && patch -p1 -N < $BS/patches/gcc-16.1.0-1.patch) || true
  touch $BLD/patched
fi

if [ ! -f $BLD/done-binutils ]; then
  step binutils
  mkdir -p $BLD/binutils && cd $BLD/binutils
  $SRC/binutils/configure --prefix=$prefix --target=$target --disable-nls --disable-werror \
    --enable-lto --enable-plugins --enable-poison-system-directories \
    --disable-gdb --disable-gdbserver --disable-sim --disable-gprofng --disable-libdecnumber --disable-readline
  make -j$J; make install; touch $BLD/done-binutils
fi

if [ ! -f $BLD/done-gcc1 ]; then
  step gcc stage1
  mkdir -p $BLD/gcc && cd $BLD/gcc
  $SRC/gcc/configure --target=$target --prefix=$prefix \
    --enable-languages=c,c++,lto --with-gnu-as --with-gnu-ld --with-gcc \
    --enable-cxx-flags='-ffunction-sections' --disable-libstdcxx-verbose \
    --enable-poison-system-directories --enable-threads=posix --disable-win32-registry \
    --disable-nls --disable-debug --disable-libmudflap --disable-libssp --disable-libgomp \
    --disable-libstdcxx-pch --enable-libstdcxx-time=yes --enable-libstdcxx-filesystem-ts \
    --with-newlib=yes --with-native-system-header-dir=/include --with-sysroot=$prefix/$target \
    --enable-lto --disable-tm-clone-registry --disable-__cxa_atexit --disable-bootstrap \
    --with-march=armv8 --enable-multilib --with-pkgversion="devkitA64-src"
  make -j$J all-gcc; make install-gcc; touch $BLD/done-gcc1
fi

if [ ! -f $BLD/done-newlib ]; then
  step newlib
  mkdir -p $BLD/newlib && cd $BLD/newlib
  CFLAGS_FOR_TARGET="-O2 -ffunction-sections -fdata-sections" \
  $SRC/newlib/configure --disable-newlib-supplied-syscalls --enable-newlib-mb \
    --disable-newlib-wide-orient --enable-newlib-register-fini \
    --target=$target --prefix=$prefix
  make -j$J; make install; touch $BLD/done-newlib
fi

if [ ! -f $BLD/done-gcc2 ]; then
  step gcc stage2
  cd $BLD/gcc
  make -j$J all; make install; touch $BLD/done-gcc2
fi
# GCC installs a limits.h that does not chain to newlib's when newlib's headers were not there at stage 1
# (libnx then misses PATH_MAX): rebuild it the way GCC's own fixincludes would.
GCC_INC=$prefix/lib/gcc/$target/16.1.0/include
cat $SRC/gcc/gcc/limitx.h $SRC/gcc/gcc/glimits.h $SRC/gcc/gcc/limity.h > $GCC_INC/limits.h
step devkitA64 done
$prefix/bin/$target-g++ --version | head -1
