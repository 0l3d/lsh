CC = gcc
CFLAGS = -g -O2 -march=native

LUA_LIB_STATIC = /usr/lib/liblua5.4.a

LDFLAGS = -I/usr/include/lua5.4 \
          -Wl,-E \
          -Wl,-Bstatic $(LUA_LIB_STATIC) -Wl,-Bdynamic \
          -ldl -lpthread -lc -lm

TARGET = lsh
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

