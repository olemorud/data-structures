.SUFFIXES:

CC       := gcc
BUILD    := debug
PLATFORM ?= linux

DEFINES := CACHE_LINE_SIZE="((size_t)$(shell ./get-cache-line-size.sh))"

CFLAGS.gcc         := -std=c23 -Wall -Wextra -fanalyzer -g $(addprefix -D, $(DEFINES))
CFLAGS.gcc.debug   := $(CFLAGS.gcc) -O0 -ggdb -fsanitize=address,undefined
CFLAGS.gcc.release := $(CFLAGS.gcc) -O3 -flto -march=native -DNDEBUG

CFLAGS.clang         := -std=c23 -Wall -Wextra -g $(addprefix -D, $(DEFINES))
CFLAGS.clang.debug   := $(CFLAGS.clang) -O0 -fsanitize=address,undefined
CFLAGS.clang.release := $(CFLAGS.clang) -O3 -flto -march=native -DNDEBUG

CFLAGS_IDENTIFIER := CFLAGS.$(CC).$(BUILD)
CFLAGS            := $($(CFLAGS_IDENTIFIER))

BUILD_DIR := build

all: $(BUILD_DIR)/btree

$(BUILD_DIR)/btree: btree.c arena.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $($(CFLAGS_IDENTIFIER).$*) $^ -o $@

