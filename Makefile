CC      = gcc
CFLAGS  = -Wvla -Wextra -Werror -D_GNU_SOURCE -Iinclude
LDFLAGS =

SRC_DIR   = src
BUILD_DIR = build
BIN_DIR   = bin

# Moduli comuni esistenti (attualmente solo il parser config)
COMMON_OBJS = $(BUILD_DIR)/config.o

# Unico target eseguibile abilitato per adesso
TARGETS = $(BIN_DIR)/responsabile_mensa

all: dirs $(TARGETS)

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Regola generica per .c -> .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Eseguibile del responsabile (linka il suo main + il parser)
$(BIN_DIR)/responsabile_mensa: $(BUILD_DIR)/responsabile_mensa.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)/*.o $(BIN_DIR)/*

.PHONY: all clean dirs
