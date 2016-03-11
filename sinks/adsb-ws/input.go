package main

import (
	"log"
	"os"
)

func readInput() {
	for {
		b1 := make([]byte, 256)
		n, err := os.Stdin.Read(b1)
		if err != nil {
			log.Printf("error: %v", err)
			break
		}
		log.Println(n, b1)
	}
}
