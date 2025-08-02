CC=gcc
CFLAGS= -g -O2  -march=native
LDFLAGS = -lutil -lcurl
TARGET=lsh
SRC_DIR = src
SOURCES := $(shell find $(SRC_DIR) -name '*.c')
OBJECTS := $(SOURCES:.c=.o)

all: $(TARGET) minimize

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

minimize:
	strip $(TARGET)

clean:
	rm -rf $(OBJECTS) $(TARGET)
