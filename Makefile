CC=gcc
CFLAGS= -g -O2  -march=native
LDFLAGS = -lutil
TARGET=minshell
SOURCES=minshell.c
OBJECTS=$(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -rf $(OBJECTS) $(TARGET)
