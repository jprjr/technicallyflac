.PHONY: all clean

CFLAGS = -Wall -Wextra -g -O2
LDFLAGS = 
LIBOGG_CFLAGS = $(shell pkg-config --cflags ogg)
LIBOGG_LDFLAGS = $(shell pkg-config --libs ogg)

all: example-flac example-ogg libtechnicallyflac.a libtechnicallyflac.so

libtechnicallyflac.a: technicallyflac.o
	$(AR) rcs $@ $^

libtechnicallyflac.so: technicallyflac.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

technicallyflac.o: technicallyflac.c
	$(CC) $(CFLAGS) -o $@ -c $^

example-flac: example-flac.o example-shared.o
	$(CC) -o $@ $^ $(LDFLAGS)

example-flac.o: example-flac.c ../technicallyflac.h
	$(CC) $(CFLAGS) -o $@ -c $<

example-ogg: example-ogg.o example-shared.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBOGG_LDFLAGS)

example-ogg.o: example-ogg.c ../technicallyflac.h
	$(CC) $(CFLAGS) $(LIBOGG_CFLAGS) -o $@ -c $<

example-shared.o: example-shared.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f example-flac example-flac.o example-ogg example-ogg.o example-shared.o libtechnicallyflac.a libtechnicallyflac.so technicallyflac.o
