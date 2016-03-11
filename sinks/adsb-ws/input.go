package main

import (
	"bufio"
	"errors"
	"log"
	"os"
)

func decodeVarint(r *bufio.Reader) (n uint64, err error) {
	var value uint64
	var shift uint16
	for {
		c, err := r.ReadByte()
		if err != nil {
			return 0, err
		}
		value |= (uint64(c) & 0x7f) << shift
		if c & 0x80 == 0 {
			return value, nil
		}
		shift += 7
		if shift > 21 {
			return 0, errors.New("invalid varint")
		}
	}
}

func readInput() {
	r := bufio.NewReader(os.Stdin)
	for {
		c, err := r.ReadByte()
		if err != nil {
			log.Printf("error: %v", err)
			break
		}
		if c != 0x0a {
			log.Printf("invalid message type: %v", c)
			break
		}
		msglen, err := decodeVarint(r)
		if err != nil {
			log.Printf("error: %v", err)
			break
		}
		buf := make([]byte, msglen)
		n, err := r.Read(buf)
		if err != nil {
			log.Printf("error: %v", err)
			break
		}
		if uint64(n) != msglen {
			log.Printf("short read")
			break
		}
		log.Println(buf)
	}
	os.Exit(1)
}
