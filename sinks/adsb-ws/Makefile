all: adsb-ws

proto/adsb.pb.go: ../../proto/adsb.proto
	protoc --gogoslick_out=. --proto_path=../.. $<

adsb-ws: *.go proto/adsb.pb.go
	go build

clean:
	rm -f adsb-ws
