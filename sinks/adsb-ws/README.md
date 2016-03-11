## Overview

adsb-ws takes an ads-b feed and makes it available via [websockets](https://en.wikipedia.org/wiki/WebSocket) (written in Go)

## Use

Pass `--exec-send=json=/path/to/adsb-ws` to [adsbus](../../adsbus/).

Optional flags:
* `--exec-send=json="/path/to/adsb-ws --bind-address=host:port"`
* `--exec-send=json="/path/to/adsb-ws --static-dir=/path/to/static/content"`
