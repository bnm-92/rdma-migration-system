#!/bin/bash

server_id=1
max_memory=$((1024*1024*512))
# max_memory=$((4096 * 4))
test_memory=$((1024*1024))

while [[ $test_memory -le $max_memory ]]; do
	./data_transfer ../config.txt $server_id $((test_memory)) 100 > data_$test_memory.txt
    sleep 1
	test_memory=$((test_memory*2))
done

