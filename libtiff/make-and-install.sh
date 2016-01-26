#!/bin/bash

ARCH="mv"
HOST=arm-mv5sft-linux-gnueabi

[ ! -e ./info ] && echo "ERROR! no info-file found!" && exit 1
DIR=`cat ./info`

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

INSTALL_PATH=`pwd`/$ARCH/deploy
BUILD_DIR=`pwd`/$ARCH/build

mkdir -p $BUILD_DIR
cd $BUILD_DIR

echo `pwd`

make -j4 && make install DESTDIR=$INSTALL_PATH

$HOST-strip $INSTALL_PATH/usr/local/bin/tiff2pdf
