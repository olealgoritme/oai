CC=gcc
OPTIONS=-Werror -O3
LIBS=-lcairo -lxcb -lX11 -lm
BUILD_DIR=.
SOURCES=oai.c
BINARY=oai

all:
	$(CC) $(OPTIONS) -o $(BINARY) $(SOURCES) $(LIBS)
clean:
	rm -rf $(BINARY)
install:
	cp $(BINARY) /usr/local/bin/
