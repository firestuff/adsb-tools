# adsbus

adsbus is a hub and protocol translator for [ADS-B](https://en.wikipedia.org/wiki/Automatic_dependent_surveillance_%E2%80%93_broadcast) messages.

It is conceptually similar to `dump1090 --net-only`, but supports more protocols and configurations. It doesn't talk to your radio itself; it
hooks programs that do, then handles the network distribution and format translation. It doesn't output to a web interface or send data to
services like FlightAware; it provides hooks for programs that do.


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
	* Local files or [named pipes](https://en.wikipedia.org/wiki/Named_pipe)
	* [stdin/stdout](https://en.wikipedia.org/wiki/Standard_streams)
	* Execute a command and talk to its stdin/stdout
* Data flows:
	* Send (data flows out of adsbus)
	* Receive (data flows in to adsbus)
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
	* Slight less rapid detection and disconnection of send <-> send connections
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
