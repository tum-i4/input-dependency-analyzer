#!/bin/bash


function get_graph_name () {
    str="$1"
    if [[ "$str" == *"main"* ]];
    then
        str1="${str:6}"
    else 
        str1="${str:7}"
    fi

    eval $2="${str1::-4}"
    eval $2+='.png'
}

for file in ./cfg.*.dot;
do
    #functioname="ABUSH"
    #echo "$file"
    get_graph_name $file functioname
    echo "creating $functioname"
    dot -Tpng $file -o $functioname
done
