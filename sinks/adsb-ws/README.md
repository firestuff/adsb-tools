## Overview

adsb-ws takes an ADS-B feed and makes it available via [websockets](https://en.wikipedia.org/wiki/WebSocket) (written in Go)

This exposes data from adsbus to HTTP clients. It allows you to write clients that consume the data entirely in JavaScript.

## Use

Pass `--exec-send=json="exec /path/to/adsb-ws"` to [adsbus](../../adsbus/).

Optional flags:
* `--exec-send=json="exec /path/to/adsb-ws --bind-address=host:port"`
* `--exec-send=json="exec /path/to/adsb-ws --static-dir=/path/to/static/content"`
