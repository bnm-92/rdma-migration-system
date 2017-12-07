#!/bin/bash

if [ $# -lt 1 ];then
    echo "Arguments missing"
    exit 1
fi

server_id=$1
max_memory=$((16*1024*1024))
total_memory=$((67108864))
test_memory=4096

while [ "$test_memory" -le "$max_memory" ]
do
    ./expPagingUniform ../config.txt $server_id $total_memory $test_memory
    test_memory=$((test_memory*2))
done