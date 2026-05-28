CC       = gcc
CFLAGS   = -Wvla -Wextra -Werror -D_GNU_SOURCE -Iinclude
LDFLAGS  =

SRC_DIR   = src
BUILD_DIR = build
BIN_DIR   = bin

# Common modules (compiled as .o and linked into every executable)
COMMON_OBJS = $(BUILD_DIR)/config.o $(BUILD_DIR)/ipc_utils.o \
              $(BUILD_DIR)/time_utils.o $(BUILD_DIR)/stats.o

# Executables
TARGETS = $(BIN_DIR)/responsabile_mensa \
          $(BIN_DIR)/operatore \
          $(BIN_DIR)/utente

all: dirs $(TARGETS)

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Generic rule: .c -> .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Executables (each links its own .o + common modules)
$(BIN_DIR)/responsabile_mensa: $(BUILD_DIR)/responsabile_mensa.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/operatore: $(BUILD_DIR)/operatore.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)


$(BIN_DIR)/utente: $(BUILD_DIR)/utente.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)/*.o $(BIN_DIR)/*

.PHONY: all clean dirs
