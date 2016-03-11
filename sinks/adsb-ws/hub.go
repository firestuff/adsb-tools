package main

type hub struct {
	connections map[*connection]bool
	broadcast   chan []byte
	register    chan *connection
	unregister  chan *connection
	greeting    []byte
}

var h = hub{
	broadcast:   make(chan []byte),
	register:    make(chan *connection),
	unregister:  make(chan *connection),
	connections: make(map[*connection]bool),
}

func (h *hub) run() {
	for {
		select {
		case c := <-h.register:
			h.connections[c] = true
			c.send <- h.greeting

		case c := <-h.unregister:
			_, ok := h.connections[c]
			if ok {
				delete(h.connections, c)
				close(c.send)
			}

		case m := <-h.broadcast:
			if len(h.greeting) == 0 {
				h.greeting = m
			}
			for c := range h.connections {
				select {
				case c.send <- m:
				default:
					close(c.send)
					delete(h.connections, c)
				}
			}
		}
	}
}
