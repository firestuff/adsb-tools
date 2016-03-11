## Overview

adsb-tools is a suite of tools, written in several different languages, for capturing, processing, transmitting, and using [ADS-B](https://en.wikipedia.org/wiki/Automatic_dependent_surveillance_%E2%80%93_broadcast) data.


## Components

* [adsbus](adsbus/) is a network hub and protocol converter for ADS-B messages (written in C)
* Sources (capture/generate data)
* Sinks (consume/use data)
	* [adsb-ws](adsb-ws/) takes an ads-b feed and makes it available via [websockets](https://en.wikipedia.org/wiki/WebSocket) (written in Go)
	* [sourcestats](sourcestats/) takes an ads-b feed and periodically outputs statistics on message and clock rate (written in Python)
