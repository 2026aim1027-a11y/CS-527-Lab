CC = gcc

CFLAGS = -Wall -Wextra -std=c11

TARGET = simulator

SRCS = main.c compiler.c processor.c memory.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q *.o *.exe *.byte 2>nul || true

rebuild: clean all

run: $(TARGET)
	.\$(TARGET).exe