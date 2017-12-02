#!/bin/bash

make clean
make
FILE="benchmarks.txt"
rm results.log output_lru.log output_crc.log
let c=1
while IFS= read line
do
	echo "################## BENCHMARK NUMBER $c ##############################"
	# Running LRU Policy
	export DAN_POLICY=0; 
	./efectiu traces/"$line.trace.gz" > output_lru.log
        # Now extract IPC from output.txt
	last_line=$(awk '/./{line=$0} END{print line}' output_lru.log)
	arr=($last_line)
	lru_ipc=${arr[2]}
	echo "LRU IPC for $line = $lru_ipc"

	# Running CONTESTANT Policy
	export DAN_POLICY=2; 
	./efectiu traces/"$line.trace.gz" > output_crc.log
        # Now extract IPC from output.txt
	last_line=$(awk '/./{line=$0} END{print line}' output_crc.log)
	arr=($last_line)
	my_ipc=${arr[2]}
	echo "CRC IPC for $line = $my_ipc"
	echo "$line $lru_ipc $my_ipc" >> results.log
	
	c=$((c + 1));
done < "$FILE"

python calc_gmean.py
