CC = gcc

CFLAGS = -std=c11 \
         -D_POSIX_C_SOURCE=200809L \
         -Wall \
         -Wextra \
         -Werror \
         -Iinclude

LDFLAGS = -pthread

TARGET = build/tcpchat

SRC = \
	src/main.c \
	src/log.c \
	src/net.c \
	src/linebuf.c \
	src/proto.c \
	src/registry.c \
	src/session.c \
	src/server.c

OBJ = $(SRC:src/%.c=build/%.o)

.PHONY: all debug asan tsan test clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS += -g -O0
debug: clean all

asan: CFLAGS += -g -O1 -fsanitize=address,undefined
asan: LDFLAGS += -fsanitize=address,undefined
asan: clean all

tsan: CFLAGS += -g -O1 -fsanitize=thread
tsan: LDFLAGS += -fsanitize=thread
tsan: clean all

test: all

clean:
	$(RM) $(OBJ) $(TARGET)
