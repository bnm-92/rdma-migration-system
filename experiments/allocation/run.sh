#!/bin/bash

max_memory=$((1024*1024*1024))
test_memory=$((1024*1024))

while [ "$test_memory" -le "$max_memory" ]
do
    ./allocation ./config.txt $test_memory
    test_memory=$((test_memory*2))
done