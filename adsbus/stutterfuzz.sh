#!/bin/bash

set -emx
trap 'kill $(jobs -p)' EXIT

valgrind --log-file=stutterfuzz-valgrind.log --track-fds=yes --show-leak-kinds=all --leak-check=full \
	./adsbus \
	--stdout=stats \
	--log-file=stutterfuzz-adsbus.log \
	--listen-send-receive=airspy_adsb=::/20000 \
	--listen-send-receive=beast=::/20001 \
	--listen-send-receive=json=::/20002 \
	--listen-send-receive=proto=::/20003 \
	--listen-send-receive=raw=::/20004 &

sleep 5

tail --follow stutterfuzz-valgrind.log &

stutterfuzz --blob-dir testcase --host=::1 --port=20000 --cycle-ms=150 --num-conns=10 2>/dev/null &
stutterfuzz --blob-dir testcase --host=::1 --port=20001 --cycle-ms=150 --num-conns=10 2>/dev/null &
stutterfuzz --blob-dir testcase --host=::1 --port=20002 --cycle-ms=150 --num-conns=10 2>/dev/null &
stutterfuzz --blob-dir testcase --host=::1 --port=20003 --cycle-ms=150 --num-conns=10 2>/dev/null &
stutterfuzz --blob-dir testcase --host=::1 --port=20004 --cycle-ms=150 --num-conns=10 2>/dev/null &

fg %1
kill %2
wait
