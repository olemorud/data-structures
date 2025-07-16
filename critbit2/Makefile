
CC     := gcc
CFLAGS := -std=c23 -g3 -Og -MMD

BUILD_DIR := build
SRC       := critbit.c arena.c test-critbit.c
DEPENDS   := $(addprefix $(BUILD_DIR)/, $(SRC:.c=.d))
OBJ       := $(addprefix $(BUILD_DIR)/, $(SRC:.c=.o))

all: $(BUILD_DIR)/test-critbit

$(BUILD_DIR)/test-critbit: $(OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

-include $(DEPENDS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $($(CFLAGS_IDENTIFIER).$*) -c $< -o $@

clean:
	rm -f $(OBJ) $(PLATFORM)

