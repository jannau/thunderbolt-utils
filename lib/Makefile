# SPDX-License-Identifier: GPL-2.0

# Makefile for thunderbolt-utils (lstbt)

# Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
# Copyright (C) 2023 Intel Corporation

INSTALL_PATH=/usr/bin

LIBTBT_EXEC = lstbt

CC = gcc
INSTALL = install
RM = rm -f

DEBUG_FLAGS = -g
WARN_FLAGS = -Wall -W
OPTIMIZE_FLAGS = -O2
CFLAGS = $(DEBUG_FLAGS) $(WARN_FLAGS) $(OPTIMIZE_FLAGS)

SRC_FILES = lstbt.c lstbt_t.c lstbt_r.c lstbt_v.c router.c adapter.c \
	    helpers.c ../utils.c
O_FILES = $(SRC_FILES:%.c=%.o)

all: $(LIBTBT_EXEC)

$(LIBTBT_EXEC): $(O_FILES)
	$(CC) $(CFLAGS) -o $@ $^ -lm

install: $(LIBTBT_EXEC)
	$(INSTALL) -d -m 755 $(DESTDIR)$(INSTALL_PATH)
	$(INSTALL) $(LIBTBT_EXEC) $(DESTDIR)$(INSTALL_PATH)

uninstall:
	$(RM) $(DESTDIR)$(INSTALL_PATH)/$(LIBTBT_EXEC)

clean:
	-$(RM) $(LIBTBT_EXEC) $(O_FILES)

.PHONY: all clean install uninstall
