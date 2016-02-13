# Airspy ADSB protocol

## Ports

* ??: server -> client feed

## Format

Textual format, similar to [raw](raw.md), but extended with MLAT and signal
level data. Unlike raw format, lines are terminated with `\r\n` (`0x0d 0x0a`).


## Frame structure
* `*` (`0x2a`)
* 7 or 14 byte frame (see [raw](raw.md))
* `;` (`0x3b`)
* 4 byte MLAT timestamp (see below)
* `;` (`0x3b`)
* 1 byte MLAT precision (see below)
* `;` (`0x3b`)
* 2 byte big-endian RSSI
* `;\r\n` (`0x3b 0x0d 0x0a`)
  

## MLAT timestamp
The MLAT timestamp included in each frame is the big-endian value of a counter
at the time of packet reception. The counter runs at a rate determined by the
precision value multiplied by 2 MHz (e.g. `0x0a` means `10 * 2 MHz = 20 MHz`).
This counter isn't calibrated to external time, but receiving software can
calculate its offset from other receiving stations across multiple packets, and
then use the differences between station receive timing to calculate signal
source position.


## Examples

* `*5DA7DA1CE30DE5;D03B5A4B;0A;7AF3;\r\n`
  * `*` (`0x2a`): Frame start
  * `5DA7DA1CE30DE5`: Mode-S short data
    * Decoded: `0x5d 0xa7 0xda 0x1c 0xe3 0x0d 0xe5`
  * `;` (`0x3b`): Delimiter
  * `D03B5A4B`: MLAT counter
    * Decoded: `0xd0 0x3b 0x5a 0x4b`
    * Decimal: 3493550667
  * `;` (`0x3b`): Delimiter
  * `0A`: MLAT precision
    * Decoded: `0x0a`
    * Decimal: 10
    * 10 * 2 MHz = 20 MHz MLAT counter clock
  * `;` (`0x3b`): Delimiter
  * `7AF3`: RSSI
    * Decoded: `0x7a 0xf3`
    * Decimal: 31475
    * 31475 / 65536 * 100% = 48%
  * `;` (`0x3b`): Delimiter
  * `\r\n` (`0x0d 0x0a`): Frame end

* `*8DA07CD89915908778A01E4B4C86;D03D33F9;0A;8437;\r\n`


## Implementations

* [Airspy ADSB decoder](http://airspy.com/download/)
