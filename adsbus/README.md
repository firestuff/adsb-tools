# adsbus

adsbus is a hub and protocol translator for [ADS-B](https://en.wikipedia.org/wiki/Automatic_dependent_surveillance_%E2%80%93_broadcast) messages.

It is conceptually similar to `dump1090 --net-only`, but supports more protocols and configurations. It doesn't talk to your radio itself; it
hooks programs that do, then handles the network distribution and format translation. It doesn't output to a web interface or send data to
services like FlightAware; it provides hooks for programs that do.

For programs to feed data into and consume data from adsbus, see the documentation on the [adsb-tools suite](../README.md).


## Building

```bash
sudo apt-get -y install build-essential git clang libjansson-dev libprotobuf-c-dev protobuf-c-compiler
git clone https://github.com/flamingcowtv/adsb-tools.git
cd adsb-tools/adsbus
make
```


## Features

* Separates the concepts of transport, data flow, and format
* Transports:
	* Outgoing TCP connection
	* Incoming TCP connection
	* Local files, [named pipes](https://en.wikipedia.org/wiki/Named_pipe), and [character devices](https://en.wikipedia.org/wiki/Device_file#Character_devices)
	* [stdin/stdout](https://en.wikipedia.org/wiki/Standard_streams)
	* Execute a command and talk to its stdin/stdout, proxy logs from its stderr
* Data flows:
	* Send (data flows out of adsbus)
	* Receive (data flows in to adsbus)
	* Send & receive (both directions on the same socket, without echo)
* Formats:
	* [airspy_adsb](../protocols/airspy_adsb.md) (a.k.a. ASAVR)
	* [beast](../protocols/beast.md)
	* [json](../protocols/json.md)
	* [proto](../protocols/proto.md) (a.k.a. ProtoBuf, Protocol Buffers)
	* [raw](../protocols/raw.md) (a.k.a. AVR)
	* stats (send only, summary aggregated data)
* Transport features:
	* [IPv4](https://en.wikipedia.org/wiki/IPv4) and [IPv6](https://en.wikipedia.org/wiki/IPv6) support
	* Reresolution and reconnection on disconnect, with backoff and jitter
	* [TCP keepalives](https://en.wikipedia.org/wiki/Keepalive#TCP_keepalive) for dead connection detection
	* [TCP fast open](https://en.wikipedia.org/wiki/TCP_Fast_Open) for faster startup of high-latency connections
	* [SO_REUSEPORT](https://lwn.net/Articles/542629/) for zero-downtime updates
* Data flow features:
	* Rapid detection and disconnection of receive <-> receive connections
	* Less rapid detection and disconnection of send <-> send connections
	* Hop counting and limits (json and proto formats only) to stop infinite routing loops
* Format features:
	* Autodetection of received data format
	* [MLAT](https://en.wikipedia.org/wiki/Multilateration) scaling for different clock rates and counter bit widths
	* [RSSI](https://en.wikipedia.org/wiki/Received_signal_strength_indication) scaling for different bit widths
	* Introduces json format for balanced human and machine readability with forward compatibility
	* Introduces proto format for fast serialization and deserialization with forward compatibility
* Federation:
	* Federation allows linking multiple instances of adsbus for:
		* Scalability (cores, number of input or output clients, etc.)
		* Efficient long-haul links (hub and spoke models on both ends)
	* json and proto formats carry information about original source across multiple hops
	* SO_REUSEPORT allows multiple adsbus instances to accept connections on the same IP and port without a load balancer


## Use
* As a commandline utility
	* For captures from a network source:

		```console
		$ ./adsbus --quiet --connect-receive=10.66.0.75/30005 --file-write=beast=dump.beast
		^C
		$ ls -l dump.beast
		-rw------- 1 flamingcow flamingcow 4065 Mar 11 15:07 dump.beast
		```

	* For file type conversion:

		```console
		$ ./adsbus --quiet --file-read=dump.beast --file-write=proto=dump.proto
		$ ls -l dump.*
		-rw------- 1 flamingcow flamingcow  4065 Mar 11 15:07 dump.beast
		-rw------- 1 flamingcow flamingcow 16548 Mar 11 15:10 dump.proto
		```

	* For examining file contents:

		```console
 		$ ./adsbus --quiet --file-read=dump.proto --stdout=json
 		{"type": "header", "server_version": "https://github.com/flamingcowtv/adsb-tools#1", "magic": "aDsB", "server_id": "0cd53a31-e62f-4c89-a969-cf0e0f7b141a", "rssi_max": 4294967295, "mlat_timestamp_mhz": 120, "mlat_timestamp_max": 9223372036854775807}
 		{"payload": "200016171BA2BB", "hops": 2, "mlat_timestamp": 370512307133580, "type": "Mode-S short", "source_id": "237e62d7-9f77-4ee0-9025-33367f5f2fc6", "rssi": 286331153}
 		{"payload": "5D400D30A969AA", "hops": 2, "mlat_timestamp": 370512308420280, "type": "Mode-S short", "source_id": "237e62d7-9f77-4ee0-9025-33367f5f2fc6", "rssi": 858993459}
 		{"payload": "5DAAC189CD820A", "hops": 2, "mlat_timestamp": 370512310040460, "type": "Mode-S short", "source_id": "237e62d7-9f77-4ee0-9025-33367f5f2fc6", "rssi": 640034342}
 		{"payload": "59A528D6148686", "hops": 2, "mlat_timestamp": 370512310059540, "type": "Mode-S short", "source_id": "237e62d7-9f77-4ee0-9025-33367f5f2fc6", "rssi": 218959117}
 		{"payload": "5DAAC189CD820A", "hops": 2, "mlat_timestamp": 370512312373740, "type": "Mode-S short", "source_id": "237e62d7-9f77-4ee0-9025-33367f5f2fc6", "rssi": 673720360}
 		...
 		```

* As a daemon
	* Using [daemontools](https://cr.yp.to/daemontools.html)
		* Does not fork/detact by default
		* Logs to stderr by default
		* Log rotation: use [multilog](https://cr.yp.to/daemontools/multilog.html)
		* Log timestamping: use [multilog](https://cr.yp.to/daemontools/multilog.html) and [tai64nlocal](https://cr.yp.to/daemontools/tai64nlocal.html)
		* Run as user: use [setuidgid](https://cr.yp.to/daemontools/setuidgid.html)
		* Shuts down cleanly on SIGTERM
	* Using other init systems
		* Use `--detach` to fork/detach
		* Use `--log-file=PATH` to write logs to a file instead of stderr
		* Use `--pid-file=PATH` to write post-detach process ID to a file
		* Log rotation: adsbus will reopen its log file on receiving SIGHUP; use with most log rotation systems
		* Log timestamping: use `--log-timestamps`
		* Run as user: use [start-stop-daemon](http://manpages.ubuntu.com/manpages/vivid/man8/start-stop-daemon.8.html), etc.
		* Shuts down cleanly on SIGTERM
	* **DO NOT RUN AS ROOT**.
		* To bind privileged (< 1024) ports, use [capabilities](http://man7.org/linux/man-pages/man7/capabilities.7.html):

			```console
			$ setcap cap_net_bind_service=+ep /path/to/adsbus
			```

		* To allow subprograms (those run with --exec-*) to take privileged actions, set capabilties on them, and consider limiting who can execute them with filesystem permissions.


## Security, reliability, testing
* Secure build options by default
	* -Weverything -Werror -pedantic-errors (with limited specific exceptions)
	* [-D_FORTIFY_SOURCE=2](https://wiki.debian.org/Hardening#DEB_BUILD_HARDENING_FORTIFY_.28gcc.2Fg.2B-.2B-_-D_FORTIFY_SOURCE.3D2.29)
	* [-fstack-protector-strong](https://wiki.debian.org/Hardening#DEB_BUILD_HARDENING_STACKPROTECTOR_.28gcc.2Fg.2B-.2B-_-fstack-protector-strong.29)
	* [-fPIE -pie](https://wiki.debian.org/Hardening#DEB_BUILD_HARDENING_PIE_.28gcc.2Fg.2B-.2B-_-fPIE_-pie.29)
	* [-z relro](https://wiki.debian.org/Hardening#DEB_BUILD_HARDENING_RELRO_.28ld_-z_relro.29)
	* [-z now](https://wiki.debian.org/Hardening#DEB_BUILD_HARDENING_BINDNOW_.28ld_-z_now.29)
* valgrind clean
	* Zero open fds and allocated blocks when run with `--track-fds=yes --show-leak-kinds=all --leak-check=full`
	* Cleans up on normal exit and when handling SIGINT/SIGTERM
* Subprogram isolation
	* All fds created with CLOEXEC; none passed on to children (tested with `--exec-receive='ls -l /proc/self/fd 1>&2'`)
	* Separate process group
* Test suite
	* `make test` runs a large set of test inputs through adsbus under valgrind
* Parser fuzzing
	* `make afl-fuzz` runs adsbus inside [american fuzzy lop](http://lcamtuf.coredump.cx/afl/) starting from previous output cases
* Network fuzzing
	* `stutterfuzz.sh` runs adsbus with [stutterfuzz](https://github.com/flamingcowtv/stutterfuzz) to test connection/network handling under load, using afl test cases
