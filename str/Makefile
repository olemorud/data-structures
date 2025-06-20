# Disable built-in rules
.SUFFIXES:

# Options for the build
# =====================
TARGET    := myprogram
SRC_DIR   := src
BUILD_DIR := build
CC        := gcc
BUILD     := debug

# Source files
# ============
SRC := foo.c bar.c baz.c main.c

# Examples of CFLAGS for different CC / BUILD combinations
# ========================================================
CFLAGS.gcc         := -MMD -Wall -Wextra -fanalyzer -g
CFLAGS.gcc.debug   := $(CFLAGS.gcc) -O1 -fsanitize=address,undefined -g3
CFLAGS.gcc.release := $(CFLAGS.gcc) -O3 -flto -DNDEBUG

CFLAGS.clang         := -MMD -Wall -Wextra -g
CFLAGS.clang.debug   := $(CFLAGS.clang) -O1 -fsanitize=address,undefined -g3
CFLAGS.clang.release := $(CFLAGS.clang) -O3 -flto -DNDEBUG

CFLAGS.x86_64-w64-mingw32-gcc         := -MMD -std=c23 -Wall -Wextra -g
CFLAGS.x86_64-w64-mingw32-gcc.debug   := $(CFLAGS.x86_64-w64-mingw32-gcc) -O1 -g3
CFLAGS.x86_64-w64-mingw32-gcc.release := $(CFLAGS.x86_64-w64-mingw32-gcc) -O3 -flto -DNDEBUG

CFLAGS := $(CFLAGS.$(CC).$(BUILD))

DEPENDS := $(addprefix $(BUILD_DIR)/, $(SRC:.c=.d))
OBJ     := $(addprefix $(BUILD_DIR)/, $(SRC:.c=.o))
TARGET  := $(addprefix $(BUILD_DIR)/, $(TARGET))

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

# Requires the -MMD flag from GCC
-include $(DEPENDS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(DEPENDS) $(TARGET)
	rmdir $(BUILD_DIR)

