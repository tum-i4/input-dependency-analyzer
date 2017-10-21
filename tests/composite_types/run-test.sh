#!/bin/bash

echo "Run loop tests"

LIB_LOC=/usr/local/lib
LOCAL_LIB_LOC=../../build/lib

rm *.bc

echo "Static array test"

clang array.c -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so array.bc -inputdep-statistics -o out.bc

if cmp stats.txt array_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

echo "Heap allocated array test"

clang heap_allocated_array.c -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so heap_allocated_array.bc -inputdep-statistics -o out.bc

if cmp stats.txt heap_allocated_array_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

echo "Multidimensional array test"

clang multidimensional_array.c -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so multidimensional_array.bc -inputdep-statistics -o out.bc

if cmp stats.txt multidimensional_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

echo "Structs test"

clang   structs.c -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so structs.bc -inputdep-statistics -o out.bc

if cmp stats.txt structs_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

echo "Classes test"

clang classes.cpp -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so classes.bc -inputdep-statistics -o out.bc

if cmp stats.txt classes_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

rm *.bc

