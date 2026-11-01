#!/usr/bin/env bash
# After build_devkita64.sh: devkita64-rules, switch-tools (elf2nro, nacptool), general-tools (bin2s),
# libnx and deko3d, all from their GitHub sources, into /opt/devkitpro.
set -euo pipefail
J=${J:-2}
SRC=$HOME/tc/src; BLD=$HOME/tc/build
export DEVKITPRO=/opt/devkitpro DEVKITA64=/opt/devkitpro/devkitA64
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH
step() { echo "=== $(date +%T) $*"; }

if [ ! -f $BLD/done-rules ]; then
  step rules
  mkdir -p $DEVKITA64; cp $SRC/devkita64-rules/base_rules $SRC/devkita64-rules/base_tools $DEVKITA64/
  touch $BLD/done-rules
fi
if [ ! -f $BLD/done-switch-tools ]; then
  step switch-tools
  (cd $SRC/switch-tools && ./autogen.sh >/dev/null 2>&1 || autoreconf -fi)
  mkdir -p $BLD/switch-tools && cd $BLD/switch-tools
  $SRC/switch-tools/configure --prefix=$DEVKITPRO/tools
  make -j$J; make install; touch $BLD/done-switch-tools
fi
if [ ! -f $BLD/done-general-tools ]; then
  step general-tools
  (cd $SRC/general-tools && (./autogen.sh >/dev/null 2>&1 || autoreconf -fi))
  mkdir -p $BLD/general-tools && cd $BLD/general-tools
  $SRC/general-tools/configure --prefix=$DEVKITPRO/tools
  make -j$J; make install; touch $BLD/done-general-tools
fi
if [ ! -f $BLD/done-dekotools ]; then
  step dekotools   # dekodef, which deko3d's build runs (needs meson, bison, flex)
  [ -d $SRC/dekotools ] || git clone -q --depth 1 https://github.com/devkitPro/dekotools $SRC/dekotools
  meson setup $BLD/dekotools $SRC/dekotools --prefix=$DEVKITPRO/tools
  ninja -C $BLD/dekotools install; touch $BLD/done-dekotools
fi
if [ ! -f $BLD/done-libnx ]; then
  step libnx
  make -C $SRC/libnx/nx -j$J; make -C $SRC/libnx/nx install; touch $BLD/done-libnx
fi
if [ ! -f $BLD/done-deko3d ]; then
  step deko3d
  make -C $SRC/deko3d -j$J; make -C $SRC/deko3d install; touch $BLD/done-deko3d
fi
step done
ls $DEVKITPRO/libnx/lib $DEVKITPRO/tools/bin
