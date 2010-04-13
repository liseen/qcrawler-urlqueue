prefix=/usr/local

.PHONY: clean


all:
	g++ urlqueue_server.c -levent -o queue_server
	g++ urlqueue_client.c -levent -o queue_client

clean:
	rm -f *.o
distclean:
	rm -f *.o queue_server queue_client
