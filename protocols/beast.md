# Beast protocol

## Ports

* 30005: server -> client feed

## Format

All data is escaped: `0x1a -> 0x1a 0x1a`. Note that synchronization is still
complex, since `0x1a 0x31` may be the start of a frame or mid-data, depending
on what preceded it. To synchronize, you must see, in order:
* != 0x1a
* 0x1a
* {0x31, 0x32, 0x33}
Escaping makes frame length for a given type variable, up to
`2 + (2 * data length)`

* Mode-AC frame
  * `0x1a 0x31`
  * 6 byte MLAT timestamp (TODO: endianness? units?)
  * 1 byte signal level (TODO: units?)
  * 2 byte Mode-AC data
* Mode-S short frame
  * `0x1a 0x32`
  * 6 byte MLAT timestamp (TODO: endianness? units?)
  * 1 byte signal level (TODO: units?)
  * 7 byte Mode-S short data
* Mode-S long frame
  * `0x1a 0x33`
  * 6 byte MLAT timestamp (TODO: endianness? units?)
  * 1 byte signal level (TODO: units?)
  * 14 byte Mode-S long data
* Status data
  * Appears to only be used by Mode-S Beast hardware later versions
  * `0x1a 0x34`
  * 6 byte MLAT timestamp (TODO: endianness? units?)
  * ?? byte status data
  * ?? byte DIP switch configuration

## Implementations

* [Mode-S Beast hardware](http://modesbeast.com/scope.html)
* [FlightAware dump1090 fork](https://flightaware.com/adsb/piaware/install)

## References

* [Original description](http://wiki.modesbeast.com/Mode-S_Beast:Data_Output_Formats)
