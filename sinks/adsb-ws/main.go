package main

import (
	"flag"
	"log"
	"net/http"
)

var bindaddr = flag.String("bind-address", ":8080", "Address to respond to HTTP requests on")
var staticdir = flag.String("static-dir", "", "Static directory to serve at /")

func main() {
	log.SetFlags(0)
	flag.Parse()
	go h.run()
	go readInput()

	http.HandleFunc("/stream", serveStream)
	if *staticdir != "" {
		fs := http.FileServer(http.Dir(*staticdir))
		http.Handle("/", fs)
	}
	err := http.ListenAndServe(*bindaddr, nil)
	if err != nil {
		log.Fatal("Error starting web server: ", err)
	}
}
