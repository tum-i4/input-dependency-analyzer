#!/bin/bash

echo "Run micro snake"


PROJ_PATH=snake
if [ ! -d "$PROJ_PATH" ]; then
    git clone https://github.com/troglobit/snake.git
fi

LIB_LOC=/usr/local/lib
LOCAL_LIB_LOC=../../../build/lib

cd $PROJ_PATH
rm *.bc

clang snake.c -DVERSION=\"1.0.1\" -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so snake.bc -inputdep-statistics -o out.bc

if cmp stats.txt ../stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

cd -
#rm -rf $PROJ_PATH

