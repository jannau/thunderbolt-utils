# Makefile for tbtutils (lstbt)

LIBTBT_EXEC = /usr/bin/lstbt

CC = gcc
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

clean:
	-$(RM) $(LIBTBT_EXEC) $(O_FILES)

