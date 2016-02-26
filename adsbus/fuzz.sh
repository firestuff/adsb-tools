#!/bin/sh
set -x
set -e

test -d findings || mkdir findings
make clean
COMP=afl-clang make
afl-fuzz -i testcase/ -o findings/ ./adsbus --stdin --stdout=airspy_adsb --stdout=beast --stdout=json --stdout=proto --stdout=raw --stdout=stats
