CC = gcc
CPPFLAGS =
CFLAGS = -O2 -g -Wall -Wextra -Wformat=2 `pkg-config --cflags glib-2.0` `xml2-config --cflags`
LDLIBS = `pkg-config --libs glib-2.0`
LIBS = `xml2-config --libs`

all: httpd

clean:
	rm -f *.o *~

distclean: clean
	rm -f httpd
