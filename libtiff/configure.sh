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

INSTALL_PATH=`pwd`/$ARCH/deploy
BUILD_DIR=`pwd`/$ARCH/build

mkdir -v -p $BUILD_DIR
cd $BUILD_DIR

[ -e ./Makefile ] && make distclean

echo `pwd`

../../$DIR/configure --host=$HOST --target=$HOST CC=$HOST-gcc STRIP=$HOST-strip CXX=$HOST-g++ --disable-shared --disable-dependency-tracking --disable-jpeg --disable-jbig --disable-lzma

