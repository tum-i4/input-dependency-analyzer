#!/bin/bash

echo "Run loop tests"

LIB_LOC=/usr/local/lib
LOCAL_LIB_LOC=../../build/lib

rm *.bc

echo "Input argument dependencies test"

clang input_argument_dependents.cpp -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so input_argument_dependents.bc -inputdep-statistics -o out.bc

if cmp stats.txt input_argument_deps_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

echo "Input dependencies test"

clang input_dependents.cpp -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so input_dependents.bc -inputdep-statistics -o out.bc

if cmp stats.txt input_deps_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

echo "Input independent test"

clang input_independents.cpp -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so input_independents.bc -inputdep-statistics -o out.bc

if cmp stats.txt input_indeps_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

echo "Mixed dependencies test"

clang mixed_dependencies.cpp -c -emit-llvm

opt -load $LOCAL_LIB_LOC/libInputDependency.so mixed_dependencies.bc -inputdep-statistics -o out.bc

if cmp stats.txt mixed_deps_stats_gold.txt; then
    echo "PASS"
else
    echo "FAIL"
fi

rm *.bc

