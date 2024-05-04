CC = gcc
CFLAGS= -std=gnu99 -g -Wall -Wextra -Werror -pedantic -pthread -lrt

.PHONY: all clean

all: proj2

proj2: proj2.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f proj2

zip:
	zip -r proj2.zip proj2.c Makefile
