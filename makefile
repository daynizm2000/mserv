CC     := gcc
CFLAGS := -Wall -Wextra -I./include
LDFLAGS:= -ldl -lssl -lcrypto -lllhttp
TARGET := mserv

SRCS   := main.c $(wildcard src/*.c)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)