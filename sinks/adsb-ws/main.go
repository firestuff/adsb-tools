package main

import (
	"flag"
	"log"
	"net/http"
)

var bindaddr = flag.String("bind-address", ":8080", "Address to respond to HTTP requests on")

func main() {
	log.SetFlags(0)
	flag.Parse()
	go h.run()
	go readInput()
	http.HandleFunc("/stream", serveStream)
	err := http.ListenAndServe(*bindaddr, nil)
	if err != nil {
		log.Fatal("Error starting web server: ", err)
	}
}
