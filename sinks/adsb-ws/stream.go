package main

import (
	"log"
	"net/http"

	"github.com/gorilla/websocket"
)

type connection struct {
	ws *websocket.Conn
	send chan []byte
}

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

func (c *connection) writePump() {
	for {
		message, ok := <-c.send
		if !ok {
			c.ws.WriteMessage(websocket.CloseMessage, []byte{})
			return
		}
		err := c.ws.WriteMessage(websocket.TextMessage, message)
		if err != nil {
			return
		}
	}
}

func serveStream(w http.ResponseWriter, r *http.Request) {
	ws, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("%s: Error in websocket handshake: %s", r.RemoteAddr, err)
		return
	}
	log.Printf("%s: New connection", r.RemoteAddr)
	c := &connection{send: make(chan []byte, 256), ws: ws}
	h.register <- c
	c.writePump()
	h.unregister <- c
	c.ws.Close()
	log.Printf("%s: Connection closed", r.RemoteAddr)
}
