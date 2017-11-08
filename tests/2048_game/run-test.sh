#!/bin/bash

echo "Run 2048 game"


PROJ_PATH=2048_game
if [ ! -d "$PROJ_PATH" ]; then
    git clone https://github.com/cuadue/2048_game.git
fi

LIB_LOC=/usr/local/lib
LOCAL_LIB_LOC=../../../build/lib

cd $PROJ_PATH
rm *.bc

clang 2048_game.c -DVERSION=\"1.0.1\" -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so 2048_game.bc -inputdep-statistics -o out.bc

if cmp stats.txt ../stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

cd -
#rm -rf $PROJ_PATH

