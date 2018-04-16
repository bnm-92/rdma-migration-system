#!/bin/bash

if [ $# -lt 1 ];then
    echo "Arguments missing"
    exit 1
fi

server_id=$1
max_memory=$((512*1024*1024))
iter=10
test_memory=4096

while [ "$test_memory" -le "$max_memory" ]
do
    ./expRegionTransfer ./config.txt $server_id $test_memory $iter
    test_memory=$((test_memory*2))
done
