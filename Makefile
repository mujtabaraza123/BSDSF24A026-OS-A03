CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude
TARGET = bin/myshell
OBJECTS = obj/main.o obj/shell.o obj/execute.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

obj/%.o: src/%.c include/shell.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p obj

clean:
	rm -f $(OBJECTS) $(TARGET)
