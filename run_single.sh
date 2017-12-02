#!/bin/sh

make clean
make

export DAN_POLICY=0; 
./efectiu $1 

export DAN_POLICY=2;
./efectiu $1

