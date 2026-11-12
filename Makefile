CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude
LDFLAGS = -lreadline
TARGET = bin/myshell
OBJECTS = obj/main.o obj/shell.o obj/execute.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

obj/%.o: src/%.c include/shell.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p obj

clean:
	rm -f $(OBJECTS) $(TARGET)
