prefix=/usr/local

.PHONY: all clean distclean

CC=g++
CPPFLAGS=-g -o0

SOURCES		= url_queue_server.cpp
OBJECTS		= $(SOURCES:.cpp=.o)
MODULES		= liburlqueue.so

all: urlqueued $(MODULES)

urlqueued: $(OBJECTS)
	$(CC) $(CPPFLAGS) -o $@ $^ -levent

$(OBJECTS): url_queue_common.h

$(MODULES): url_queue_client.cpp
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<
	
clean:
	rm -f *.o

distclean:
	rm -f *.o $(MODULES) urlqueued

install: all
	test -d $(prefix) || mkdir -p $(prefix)
	test -d $(prefix)/bin || mkdir -p $(prefix)/bin
	test -d $(prefix)/include || mkdir -p $(prefix)/include
	test -d $(prefix)/lib || mkdir -p $(prefix)/lib
	cp urlqueued $(prefix)/bin/
	cp *.h $(prefix)/include/
	cp $(MODULES) $(prefix)/lib/
