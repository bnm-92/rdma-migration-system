#!/bin/bash

if [[ $# -lt 2 ]];then
	echo "Arguments missing"
	exit 1
fi

server_id=$1
test_memory=$2

for i in {1..5};do
	test_memory=$test_memory*2
	for j in {1..10};do
		./test_RDMAMemoryTransfer ./config.txt $server_id $((test_memory))
		sleep 1
  done
done

