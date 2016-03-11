package main

import (
	"flag"
	"log"
	"net/http"
	"os"
	"runtime"
)

// #cgo LDFLAGS: -lcap
// #include <assert.h>
// #include <stdlib.h>
// #include <sys/capability.h>
// const char *get_caps_str() {
//	cap_t caps = cap_get_proc();
//	assert(caps);
//	char *caps_str = cap_to_text(caps, NULL);
//	assert(caps_str);
//	assert(!cap_free(caps));
//	return caps_str;
// }
import "C"

var bindaddr = flag.String("bind-address", ":8080", "Address to respond to HTTP requests on")
var staticdir = flag.String("static-dir", "", "Static directory to serve at /")

func main() {
	log.SetFlags(0)
	log.SetPrefix("{adsb-ws} ")
	flag.Parse()

	caps_str := C.get_caps_str()
	log.Printf("Runtime data:")
	log.Printf("\tgo_version: %s", runtime.Version())
	log.Printf("\tgo_compiler: %s", runtime.Compiler)
	log.Printf("\tgo_arch: %s", runtime.GOARCH)
	log.Printf("\tgo_os: %s", runtime.GOOS)
	log.Printf("\tprocess_id: %d", os.Getpid())
	log.Printf("\tcapabilities: %s", C.GoString(caps_str))

	go h.run()
	go readInput()

	http.HandleFunc("/stream", serveStream)
	if *staticdir != "" {
		fs := http.FileServer(http.Dir(*staticdir))
		http.Handle("/", fs)
		log.Printf("Serving static content from: %s", *staticdir)
	}
	log.Printf("Listening on: %s", *bindaddr)
	err := http.ListenAndServe(*bindaddr, nil)
	if err != nil {
		log.Fatal("Error starting web server: ", err)
	}
}
