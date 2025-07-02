CC = gcc
CFLAGS = -Wall -O2
LIBS = -lcurl

all: cover-scraper

cover-scraper: cover-scraper.c
	$(CC) $(CFLAGS) cover-scraper.c -o cover-scraper $(LIBS)

clean:
	rm -f cover-scraper
