prefix=/opt/qcrawler-thirdparty

.PHONY: all clean distclean

CC=g++
CPPFLAGS=-g -O2 -I/opt/qcrawler-thirdparty/include
LIBS=-L/opt/qcrawler-thirdparty/lib

MODULES		= liburlqueue.so

all: urlqueued $(MODULES)

urlqueued: url_queue_server.cpp
	$(CC) $(CPPFLAGS) -o $@ $^ $(LIBS) -levent


$(OBJECTS): url_queue_common.h

$(MODULES): url_queue_client.cpp
	$(CC) $(CPPFLAGS) $(LIBS) -levent -lmemcached -fPIC -shared -o $@ $<
	
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
