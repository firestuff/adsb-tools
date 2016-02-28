# Raw protocol

Sometimes known as "AVR".

## Ports

* 30001: client -> server feed
* 30002: server -> client feed

## Format

Simple textual format.


## Frame structure
* `*` (`0x2a`)
* Uppercase hex-encoded 2, 7, or 14 byte frame (4, 14, or 28 bytes after encoding)
* `;` (`0x3b`)
* `\n` (`0x0a`) **OR** `\r\n` (`0x0d 0x0a`)
  

## Examples
* `*96A328343B1CA6AB3CE9AD87897A;`
  * `*` (`0x2a`): Frame start
  * `96A328343B1CA6AB3CE9AD87897A`: Mode-S long data
    * Decoded: `0x96 0xa3 0x28 0x34 0x3b 0x1c 0xa6 0xab 0x3c 0xe9 0xad 0x87 0x89 0x7a`
  * `;` (`0x3b`): Delimiter
  * `\n` (`0x0a`): Frame end

* `*59A880E10968EF;`
  * `*` (`0x2a`): Frame start
  * `59A880E10968EF`: Mode-S short data
    * Decoded: `0x59 0xa8 0x80 0xe1 0x09 0x68 0xef`
  * `;` (`0x3b`): Delimiter
  * `\n` (`0x0a`): Frame end


## Implementations

* [antirez dump1090](https://github.com/antirez/dump1090)
* [MalcolmRobb dump1090](https://github.com/MalcolmRobb/dump1090)
* [mutability dump1090](https://github.com/mutability/dump1090)
* [PiAware dump1090](https://flightaware.com/adsb/piaware/install)
