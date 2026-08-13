CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

SRCS = main.c compiler.c processor.c memory.c
OBJS = $(SRCS:.c=.o)
TARGET = simulator

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) program.byte data.byte

.PHONY: all clean
