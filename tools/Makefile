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
CONFIGURE       := $(ROOT)/configure.sh
DEPS            := $(OBJECTS:%.o=%.d)

QUIET ?= 1
ifeq ($(strip $(QUIET)),1)
QUIET_CHAR = @
else
QUIET_CHAR =
endif

all: $(TARGET)

$(XIPFS_LIB):
	$(QUIET_CHAR)$(CONFIGURE) --board=$(BOARD)
	$(QUIET_CHAR)$(MAKE) -C $(ROOT) BOARD=$(BOARD) DEBUG=$(DEBUG)

build:
	$(QUIET_CHAR)mkdir -p $@

bin:
	$(QUIET_CHAR)mkdir -p $@

build/%.o: src/%.c | build
	$(QUIET_CHAR)$(CC) $(CFLAGS) -MD -MP -MF $(patsubst %.o,%.d, $@) -c $< -o $@


-include $(DEPS)

$(TARGET): $(OBJECTS) $(XIPFS_LIB) | bin
	$(QUIET_CHAR)$(CC) $(OBJECTS) $(XIPFS_LIB) -o $@

clean:
	$(QUIET_CHAR)$(RM) -r build bin

.PHONY: all clean
