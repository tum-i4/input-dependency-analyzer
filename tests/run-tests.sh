#!/bin/bash

directories="2048_game
             micro_snake
             snake_c
             tetris
             bubble_sort
             control_flow
             loop_controlflow"


for dir in $directories
do
    cd $dir
    sh ./run-test.sh
    cd -
    echo
done


