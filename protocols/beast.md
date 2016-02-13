# Beast protocol

## Ports

* 30005: server -> client feed

## Format

All data is escaped: `0x1a` -> `0x1a 0x1a`. Note that synchronization is still
complex, since `0x1a 0x31` may be the start of a frame or mid-data, depending
on what preceded it. To synchronize, you must see, in order:
* != `0x1a`
* `0x1a`
* {`0x31`, `0x32`, `0x33`}

Escaping makes frame length for a given type variable, up to
`2 + (2 * data_length_sum)`


## Frame structure
* `0x1a`
* 1 byte frame type (see types below)
* 6 byte MLAT timestamp (see below)
  

## Frame types
* `0x31`: Mode-AC frame
  * 1 byte signal level (TODO: units?)
  * 2 byte Mode-AC data
* `0x32`: Mode-S short frame
  * 1 byte signal level (TODO: units?)
  * 7 byte Mode-S short data
* `0x33`: Mode-S long frame
  * 1 byte signal level (TODO: units?)
  * 14 byte Mode-S long data
* `0x34`: Status data
  * *Appears to only be used by Mode-S Beast hardware later versions*
  * ?? byte status data
  * ?? byte DIP switch configuration


## MLAT timestamp
The MLAT timestamp included in each frame is the big-endian value of a 12 MHz
counter at the time of packet reception. This counter isn't calibrated to
external time, but receiving software can calculate its offset from other
receiving stations across multiple packets, and then use the differences between
station receive timing to calculate signal source position.


## Examples

* `1a 32 08 3e 27 b6 cb 6a 1a 1a 00 a1 84 1a 1a c3 b3 1d`
  * `1a`: frame start
  * `32`: Mode-S short frame
  * `08 3e 27 b6 cb 6a`: MLAT counter value
    * Decimal: 9063047285610
  * `1a 1a': Signal level
    * Unescaped: `1a`
  * `00 a1 84 1a 1a c3 b3 1d`: Mode-S short data
    * Unescaped: `00 a1 84 1a c3 b3 1d`


## Implementations

* [Mode-S Beast hardware](http://modesbeast.com/scope.html)
* [FlightAware dump1090 fork](https://flightaware.com/adsb/piaware/install)


## References

* [Original description](http://wiki.modesbeast.com/Mode-S_Beast:Data_Output_Formats)
