\CC = gcc

CFLAGS = -Wall -Wextra -I../include
LDFLAGS = -shared

SOURCES := $(wildcard *.c)
TARGETS := $(SOURCES:.c=.so)

all: $(TARGETS)

%.so: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)