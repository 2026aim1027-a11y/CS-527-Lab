CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc -D_POSIX_C_SOURCE=200809L -Wno-stringop-truncation
SRC = src/main.c src/compiler.c src/processor.c src/memory.c src/os.c src/load_error.c src/compat.c
OBJ = $(SRC:.c=.o)
TARGET = sim

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) os.log task_*_program.byte task_*_data.byte

.PHONY: all clean
