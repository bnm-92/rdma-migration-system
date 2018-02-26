#!/bin/bash

max_memory=$((1024*1024*1024))
# max_memory=$((4096 * 4))
test_memory=$((1024*1024))

while [[ $test_memory -le $max_memory ]]; do
	echo $test_memory
	./baseline ./config.txt $((test_memory)) 100 > base_$test_memory.txt
    sleep 1
	test_memory=$((test_memory*2))
done

