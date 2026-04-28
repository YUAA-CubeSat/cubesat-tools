# Standalone build for power_dist off-target testing.
# Uses Homebrew gcc-15 as requested.
#
# Layout: this is a UNITY BUILD. power_test.c #include's both power_sim.c
# and power_dist.c directly, so only power_test.c needs to be compiled.
# That lets the test harness reach module-internal test hooks without
# exposing them through power_dist.h.
#
# Dependencies are listed manually below so editing any .c/.h triggers a
# rebuild even though only one TU is actually compiled.

CC      = /opt/homebrew/opt/gcc/bin/gcc-15
CFLAGS  ?= -std=c11 -O0 -g -Wall -Wextra -Wno-unused-parameter \
           -Wno-unused-but-set-variable -Wno-unused-variable -I.
LDFLAGS ?=
LDLIBS  ?= -lm

BUILD_DIR = build
BIN       = power_test

# Only power_test.c compiles — it transitively includes the rest.
UNITY_TU  = power_test.c
UNITY_OBJ = $(BUILD_DIR)/power_test.o

# Anything that can change compiled output.
DEPS = power_test.c power_sim.c power_dist.c eps_extra.c \
       power_sim.h  power_dist.h eps_extra.h

.PHONY: all clean run

all: $(BIN)

$(BIN): $(UNITY_OBJ)
	$(CC) $(CFLAGS) $(UNITY_OBJ) $(LDFLAGS) $(LDLIBS) -o $@

$(UNITY_OBJ): $(DEPS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(UNITY_TU) -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN)
