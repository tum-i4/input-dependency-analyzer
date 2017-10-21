#!/bin/bash

PROJ_PATH=2048_game
if [ ! -d "$PROJ_PATH" ]; then
    git clone https://github.com/cuadue/2048_game.git
fi

LIB_LOC=/usr/local/lib
LOCAL_LIB_LOC=../../../build/lib

cd $PROJ_PATH
rm *.bc
rm *.ll
rm 2048_game
rm clone*
rm extract*
rm clone_on_extract*
rm extract_on_clone*

clang 2048_game.c -c -emit-llvm


#clone
opt -load $LOCAL_LIB_LOC/libInputDependency.so -load $LOCAL_LIB_LOC/libTransforms.so 2048_game.bc -clone-functions -o clone.bc

#extract
opt -load $LOCAL_LIB_LOC/libInputDependency.so -load $LOCAL_LIB_LOC/libTransforms.so 2048_game.bc -extract-functions -o extract.bc

#extract on clone
opt -load $LOCAL_LIB_LOC/libInputDependency.so -load $LOCAL_LIB_LOC/libTransforms.so clone.bc -extract-functions -o extract_on_clone.bc

#clone on extract
#fails on input dep analysis
opt -load $LOCAL_LIB_LOC/libInputDependency.so -load $LOCAL_LIB_LOC/libTransforms.so extract.bc -clone-functions -o clone_on_extract.bc


llvm-dis 2048_game.bc
llvm-dis clone.bc
llvm-dis extract_on_clone.bc
llvm-dis clone_on_extract.bc

clang 2048_game.bc -lncurses -o 2048_game
clang clone.bc -lncurses -o clone
clang extract.bc -lncurses -o extract
clang extract_on_clone.bc -lncurses -o extract_on_clone
clang clone_on_extract.bc -lncurses -o clone_on_extract

