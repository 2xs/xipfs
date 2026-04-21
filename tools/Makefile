ROOT            := ..
BOARD           := workstation

CC              ?= cc

CFLAGS          += -Wall
CFLAGS          += -Wextra
CFLAGS          += -Werror
CFLAGS          += -std=c11
CFLAGS          += -I$(ROOT)
CFLAGS          += -I$(ROOT)/boards/$(BOARD)
CFLAGS          += -Iinclude
ifdef DEBUG
CFLAGS          += -Og
CFLAGS          += -ggdb
endif

TARGET          := bin/mkxipfs
SOURCES         := $(wildcard src/*.c)
OBJECTS         := $(SOURCES:src/%.c=build/%.o)
XIPFS_LIB       := $(ROOT)/$(BOARD)/xipfs.a

all: $(TARGET)

$(XIPFS_LIB):
	$(MAKE) -C $(ROOT) BOARD=$(BOARD) DEBUG=$(DEBUG)

build:
	mkdir -p $@

bin:
	mkdir -p $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) $(XIPFS_LIB) | bin
	$(CC) $(OBJECTS) $(XIPFS_LIB) -o $@

clean:
	$(RM) -r build bin

.PHONY: all clean
