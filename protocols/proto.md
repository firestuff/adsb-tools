# Protobuf protocol

This protocol was created by adsb-tools. This specification is official.

## Format

[Protocol buffer](https://developers.google.com/protocol-buffers/docs/overview) encoding.
Proto definition is [here](https://github.com/flamingcowtv/adsb-tools/blob/master/proto/adsb.proto).
Stream is a series of Adsb records with length encoded in a prefix (protobuf isn't self
delimiting). The prefix is structured such that an entire stream is a valid AdsbStream
record.

First frame must always be an AdsbHeader; frames can otherwise appear in any order, including
additional headers.

## Prefix
* `0x0a`: in protobuf encoding, field #1, type 2 (length-encoded bytes); (1 << 3) | 2
* Length of packet not including prefix, encoded as a [base 128 varint](https://developers.google.com/protocol-buffers/docs/encoding#varints)
  
## Packet
See [definition file](https://github.com/flamingcowtv/adsb-tools/blob/master/proto/adsb.proto)
for details.

## Tips

To decode a stream file:
`$ protoc-c --proto_path=adsb-tools/proto --decode=AdsbStream adsb-tools/proto/adsb.proto < streamfile`
