###############################################################################
#  © Université de Lille, The Pip Development Team (2015-2024)                #
#                                                                             #
#  This software is a computer program whose purpose is to run a minimal,     #
#  hypervisor relying on proven properties such as memory isolation.          #
#                                                                             #
#  This software is governed by the CeCILL license under French law and       #
#  abiding by the rules of distribution of free software.  You can  use,      #
#  modify and/ or redistribute the software under the terms of the CeCILL     #
#  license as circulated by CEA, CNRS and INRIA at the following URL          #
#  "http://www.cecill.info".                                                  #
#                                                                             #
#  As a counterpart to the access to the source code and  rights to copy,     #
#  modify and redistribute granted by the license, users are provided only    #
#  with a limited warranty  and the software's author,  the holder of the     #
#  economic rights,  and the successive licensors  have only  limited         #
#  liability.                                                                 #
#                                                                             #
#  In this respect, the user's attention is drawn to the risks associated     #
#  with loading,  using,  modifying and/or developing or reproducing the      #
#  software by the user in light of its specific status of free software,     #
#  that may mean  that it is complicated to manipulate,  and  that  also      #
#  therefore means  that it is reserved for developers  and  experienced      #
#  professionals having in-depth computer knowledge. Users are therefore      #
#  encouraged to load and test the software's suitability as regards their    #
#  requirements in conditions enabling the security of their systems and/or   #
#  data to be ensured and,  more generally, to use and operate it in the      #
#  same conditions as regards security.                                       #
#                                                                             #
#  The fact that you are presently reading this means that you have had       #
#  knowledge of the CeCILL license and that you accept its terms.             #
###############################################################################

ifndef RIOT_CFLAGS
ifeq ("$(wildcard toolchain.mk)","")
    $(error "Run ./configure first")
endif

include toolchain.mk
include boards/$(BOARD)/toolchain.mk
endif # RIOT_CFLAGS

CC              = $(PREFIX)gcc
AR              = $(PREFIX)ar

CFLAGS          = -Wall
CFLAGS         += -Wextra
CFLAGS         += -Werror
CFLAGS         += -ffreestanding
CFLAGS         += -mthumb
CFLAGS         += $(BOARD_CFLAGS)
ifndef DEBUG
CFLAGS         += -Os
else
CFLAGS         += -Og
CFLAGS         += -ggdb
endif
CFLAGS         += -I.
ifdef RIOT_CFLAGS
CFLAGS         += $(RIOT_INCLUDES)
CFLAGS         += $(RIOT_CFLAGS)
else # RIOT_CFLAGS
CFLAGS         += -Iboards/$(BOARD)
endif # RIOT_CFLAGS

TARGET          = xipfs
SOURCES         = $(wildcard src/*.c)
OBJECTS         = $(SOURCES:.c=.o)

OBJS = $(addprefix $(BOARD)/,$(OBJECTS))

all: $(BOARD)/$(TARGET).a

$(BOARD)/src:
	mkdir -p $(BOARD)/src

$(BOARD)/src/%.c: src/%.c $(BOARD)/src
	cp $< $@

$(BOARD)/$(TARGET).a: $(OBJS)
	$(AR) rcs $@ $^

$(BOARD)/src/%.o: $(BOARD)/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

realclean: clean
	$(RM) -rf $(BOARD)

clean:
	$(RM) $(BOARD)/$(OBJECTS)

.PHONY: all realclean clean
