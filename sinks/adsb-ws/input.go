package main

import (
	"bufio"
	"log"
	"os"
)

func readInput() {
	r := bufio.NewReader(os.Stdin)
	for {
		line, err := r.ReadBytes('\n')
		if err != nil {
			log.Fatal("Input read error: ", err)
		}
		h.broadcast <- line
	}
}
