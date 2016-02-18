import json
import socket


def GetEvents(host, port):
	with socket.create_connection((host, port)) as sock:
		fh = sock.makefile()
		for line in fh:
			yield json.loads(line)
