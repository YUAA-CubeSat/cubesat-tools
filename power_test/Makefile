# Standalone build for power_dist off-target testing.
# Uses Homebrew gcc-15 as requested.

CC      = /opt/homebrew/opt/gcc/bin/gcc-15
CFLAGS  ?= -std=c11 -O0 -g -Wall -Wextra -Wno-unused-parameter \
           -Wno-unused-but-set-variable -Wno-unused-variable -I.
LDFLAGS ?=
LDLIBS  ?= -lm

BUILD_DIR = build
SRCS      = power_dist.c power_sim.c power_test.c
OBJS      = $(SRCS:%.c=$(BUILD_DIR)/%.o)
BIN       = power_test

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN)
