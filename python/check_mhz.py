#!/usr/bin/python3

import sys
import time

import adsblib


if len(sys.argv) != 3:
	print('Usage: %s <host> <port>' % sys.argv[0])

advertised_mhz = None
start_time = None
start_timestamp = None
for i, event in enumerate(adsblib.GetEvents(sys.argv[1], sys.argv[2])):
	advertised_mhz = event.get('mlat_timestamp_mhz', advertised_mhz)

	if 'mlat_timestamp' not in event:
		continue

	if not start_time:
		start_time = time.time()
		start_timestamp = event['mlat_timestamp']

	if i % 1000 == 0 and start_time and advertised_mhz:
		time_diff = time.time() - start_time
		value_diff = event['mlat_timestamp'] - start_timestamp
		measured_mhz = value_diff / time_diff / 1000000
		print(
			'\r%d samples, %d seconds, advertised: %d MHz, measured: %d MHz' %
			(i, time_diff, advertised_mhz, measured_mhz),
			end='', flush=True)

print()
