CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk4 webkitgtk-6.0)
LIBS = $(shell pkg-config --libs gtk4 webkitgtk-6.0)

all: webkit

webkit: main.c
	$(CC) main.c -o webkit $(CFLAGS) $(LIBS)

clean:
	rm -f webkit
