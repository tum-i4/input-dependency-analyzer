#!/bin/bash

echo "Run bubble sort"

LIB_LOC=/usr/local/lib
LOCAL_LIB_LOC=../../build/lib

rm *.bc

clang bubble_sort.cpp -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so bubble_sort.bc -inputdep-statistics -o out.bc

if cmp stats.txt stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

rm *.bc

