CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc
SRC = src/main.c src/compiler.c src/processor.c src/memory.c
OBJ = $(SRC:.c=.o)
TARGET = sim

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) program.byte

.PHONY: all clean
