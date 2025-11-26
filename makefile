CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = nuguri
SRCS = nuguri.c

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

.PHONY: clean
clean:
	rm -f $(TARGET)
