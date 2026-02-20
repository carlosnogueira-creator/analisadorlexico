CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = lexer

all: $(TARGET)

$(TARGET): lexer.c
	$(CC) $(CFLAGS) lexer.c -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
