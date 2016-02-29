# JSON protocol

This protocol was created by adsb-tools. This specification is official.

## Format

[JSON streaming](https://en.wikipedia.org/wiki/JSON_Streaming) encoding, line delimited.
Each line must contain a single outer JSON object; no other outer types are permitted.

First frame must always be a header; frames can otherwise appear in any order, including
additional headers.

## Common fields
* `type` (string): one of:
	* `"header"` (see [Header](#header))
	* `"Mode-AC"` (see [Packet](#packet))
	* `"Mode-S short"` (see [Packet](#packet))
	* `"Mode-S long"` (see [Packet](#packet))
  

## Header
* `type`:
	* String
	* `"header"`
* `magic`:
	* String
	* `"aDsB"`
* `server_version`:
	* String
	* Unique identifier for this server implementation
	* Recommended: `"https://url/of/source#version"`
* `server_id`:
	* String
	* Unique identifier for this server instance
	* [UUID](https://en.wikipedia.org/wiki/Universally_unique_identifier) recommended
	* 36 character limit
* `mlat_timestamp_mhz`:
	* Integer
	* MHz of the clock used in subsequent `mlat_timestamp` fields
* `mlat_timestamp_max`:
	* Integer
	* Maximum value of subsequent `mlat_timestamp` fields, at which point values are expected to wrap
* `rssi_max`:
	* Integer
	* Maximum value of subsequent `rssi` fields


## Packet
* `type`:
	* String
	* One of:
		* `"Mode-AC"` (4 byte payload, 2 bytes when decoded)
		* `"Mode-S short"` (14 byte payload, 7 bytes when decoded)
		* `"Mode-S long"` (28 byte payload, 14 bytes when decoded)
* `source_id`:
	* String
	* Unique value for the source that recorded this packet.
	* [UUID](https://en.wikipedia.org/wiki/Universally_unique_identifier) recommended
	* 36 character limit
* `mlat_timestamp`:
	* Integer
	* Value of the [MLAT](https://en.wikipedia.org/wiki/Multilateration) counter when this packet arrived at the recorder
	* Range [0, `mlat_timestamp_max`]
	* Units of 1 / (`mlat_timestamp_mhz` * 10^6) Hz
* `rssi`:
	* Integer
	* [RSSI](https://en.wikipedia.org/wiki/Received_signal_strength_indication) of the received packet at the recorder
	* Range [0, `rssi_max`]
	* Units unspecified
* `payload`:
	* Upper-case, hex-encoded
	* See `type` for length


## Examples
* `{"mlat_timestamp_mhz": 120, "type": "header", "magic": "aDsB", "server_version": "https://github.com/flamingcowtv/adsb-tools#1", "server_id": "fba76102-c39a-4c4e-af7c-ddd4ec0d45e2", "mlat_timestamp_max": 9223372036854775807, "rssi_max": 4294967295}\n`
* `{"payload": "02C58939D0B3C5", "type": "Mode-S short", "rssi": 269488144, "source_id": "f432c867-4108-4927-ba1f-1cfa71709bc4", "mlat_timestamp": 247651683709560}\n`
* `{"payload": "A8000B0B10010680A600003E4A72", "type": "Mode-S long", "rssi": 2206434179, "source_id": "f432c867-4108-4927-ba1f-1cfa71709bc4", "mlat_timestamp": 247651683777900}\n`
