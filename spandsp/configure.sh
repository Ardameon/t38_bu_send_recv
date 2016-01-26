#!/bin/bash

[ ! -e ./info ] && echo "ERROR! no info-file found!" && exit 1

CURDIR="`pwd`/.."
DIR=`cat ./info`

ARCH="mv"
HOST=arm-mv5sft-linux-gnueabi
MODE=$1

case "$MODE" in
    "mv" )
        ARCH="mv"
        HOST=arm-mv5sft-linux-gnueabi
    ;;
    "x64" | "x86_64" )
        ARCH="x86_64"
        HOST=x86_64-linux-gnu
    ;;

    * )
        echo "ERROR! Target plaform not specified! Use 'mv' by default"
    ;;
esac

[ -e ./Makefile ] && make distclean

cd $CURDIR/libtiff/
./configure.sh $ARCH
./make-and-install.sh $ARCH
cd -

export CFLAGS="-I$CURDIR/libtiff/$ARCH/deploy/usr/local/include `pkg-config --cflags-only-other $CURDIR/libtiff/$ARCH/deploy/usr/local/lib/pkgconfig/libtiff-4.pc`"
export CPPFLAGS=-I$CURDIR/libtiff/$ARCH/deploy/usr/local/include
export LDFLAGS=-L$CURDIR/libtiff/$ARCH/deploy/usr/local/lib
export LIBS=`pkg-config --libs-only-l --static $CURDIR/libtiff/$ARCH/deploy/usr/local/lib/pkgconfig/libtiff-4.pc`

INSTALL_PATH=`pwd`/$ARCH/deploy
BUILD_DIR=`pwd`/$ARCH/build

mkdir -v -p $BUILD_DIR
cd $BUILD_DIR

[ -e ./Makefile ] && make distclean

echo `pwd`

../../$DIR/configure --host=$HOST --target=$HOST CC=$HOST-gcc STRIP=$HOST-strip CXX=$HOST-g++ --disable-shared --disable-dependency-tracking

