#!/bin/bash

echo "Run tetris"


PROJ_PATH=tetris
if [ ! -d "$PROJ_PATH" ]; then
    git clone https://github.com/troglobit/tetris.git
fi

LIB_LOC=/usr/local/lib
LOCAL_LIB_LOC=../../../build/lib

cd $PROJ_PATH
rm *.bc

clang tetris.c -DVERSION=\"1.0.1\" -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so tetris.bc -inputdep-statistics -o out.bc

if cmp stats.txt ../stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

cd -
#rm -rf $PROJ_PATH

