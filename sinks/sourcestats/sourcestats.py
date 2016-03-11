#!/usr/bin/python3 -u

import collections
import fileinput
import json
import sys
import threading
import time


def Log(s):
	print('{sourcestats} %s' % s, file=sys.stderr, flush=True)


class Source(object):
	_first_event = None

	def __init__(self):
		self._count_by_type = collections.defaultdict(int)
		
	def HandleEvent(self, event):
		now = time.clock_gettime(time.CLOCK_MONOTONIC)
		if not self._first_event:
			self._first_event = event
			self._first_event_timestamp = now
		self._last_event = event
		self._last_event_timestamp = now
		self._count_by_type[event['type']] += 1

	def _GetDuration(self):
		return self._last_event_timestamp - self._first_event_timestamp

	def GetTypeRate(self, t):
		return self._count_by_type[t] / self._GetDuration()

	def GetHz(self):
		ticks = self._last_event['mlat_timestamp'] - self._first_event['mlat_timestamp']
		return ticks / self._GetDuration()


class Reader(object):
	def __init__(self):
		self._sources = collections.defaultdict(Source)
		self._input = fileinput.input()
		self._LogStats()

	def _GetEvents(self):
		for line in self._input:
			yield json.loads(line)

	def _HandleHeader(self, event):
		self._config = event

	def _LogStats(self):
		data = [
			('Source ID', 'Mode-AC', 'Mode-S short', 'Mode-S long', 'Clock rate'),
		]

		for source_id, source in self._sources.items():
			data.append((
				source_id,
				'%.02f/s' % source.GetTypeRate('Mode-AC'),
				'%.02f/s' % source.GetTypeRate('Mode-S short'),
				'%.02f/s' % source.GetTypeRate('Mode-S long'),
				'%.04f' % (source.GetHz() / (self._config['mlat_timestamp_mhz'] * 1000000)),
			))

		col_width = [max(len(row[i]) for row in data) + 2 for i in range(len(data[0]))]
		for row in data:
			Log(''.join(word.ljust(col_width[i]) for i, word in enumerate(row)))
		Log('')

		self._timer = threading.Timer(60.0, self._LogStats)
		self._timer.start()

	def Read(self):
		for event in self._GetEvents():
			if event['type'] == 'header':
				self._HandleHeader(event)
				continue
			source = self._sources[event['source_id']]
			source.HandleEvent(event)


Log('Runtime data:')
Log('\tpython_version: %s' % sys.version.replace('\n', ''))
Log('')


reader = Reader()
reader.Read()
