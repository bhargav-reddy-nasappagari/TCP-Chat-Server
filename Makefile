CC       = gcc
CFLAGS   = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude -MMD -MP
LDFLAGS  = -pthread

# Variant is chosen by the debug/asan/tsan targets; plain `make` is "release".
VARIANT ?= release

ifeq ($(VARIANT),debug)
  CFLAGS += -g -O0
else ifeq ($(VARIANT),asan)
  CFLAGS  += -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined
  LDFLAGS += -fsanitize=address,undefined
else ifeq ($(VARIANT),tsan)
  CFLAGS  += -g -O1 -fno-omit-frame-pointer -fsanitize=thread
  LDFLAGS += -fsanitize=thread
else
  CFLAGS += -O2
endif

BUILD  = build/$(VARIANT)
TARGET = $(BUILD)/tcpchat

SRC = \
	src/main.c \
	src/log.c \
	src/net.c \
	src/linebuf.c \
	src/proto.c \
	src/registry.c \
	src/session.c \
	src/server.c

OBJ = $(SRC:src/%.c=$(BUILD)/%.o)
DEP = $(OBJ:.o=.d)

.PHONY: all debug asan tsan test clean

all: $(TARGET)

debug asan tsan:
	$(MAKE) VARIANT=$@ all

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

-include $(DEP)

test: all

clean:
	$(RM) -r build
