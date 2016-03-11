all: adsb-ws

proto/adsb.pb.go: ../../proto/adsb.proto
	protoc --gofast_out=. --proto_path=../.. ../../proto/adsb.proto

adsb-ws: *.go proto/adsb.pb.go
	go build

clean:
	rm -f adsb-ws
