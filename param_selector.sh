#!/bin/bash

make clean
make
rm params_ipc.log params_table.log
let c=1

export DAN_POLICY=2; 

for i in {-10..10..1}
do
	export BYPASS=$i;
	for j in {100..130..1}
	do
		export REPLACE=$j;
		./efectiu $1 > params_ipc.log
		last_line=$(awk '/./{line=$0} END{print line}' params_ipc.log)
		arr=($last_line)
		my_ipc=${arr[2]}
		echo "$i $j $my_ipc" >> params_table.log
		c=$((c + 1));
	done
done

