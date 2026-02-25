CC = clang
LDFLAGS = -fsanitize=address -g
CFLAGS = -std=c89 -fsanitize=address -g
STRICTCFLAGS = -Wall

BIN_DIR = bin
OBJ_DIR = obj

COM_DIR = common
COM_SRC = $(wildcard $(COM_DIR)/*.c)
COM_OBJ = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(COM_SRC)))

CSPICE_DIR = cspice
CSPICE_SRC_DIR = $(CSPICE_DIR)/src
CSPICE_INC_DIR = $(CSPICE_DIR)/include
CSPICE_SRC = $(wildcard $(CSPICE_SRC_DIR)/*.c)
CSPICE_OBJ = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(CSPICE_SRC)))

SUNVEC_DIR = sunvec
SUNVEC_SRC = $(wildcard $(SUNVEC_DIR)/*.c)
SUNVEC_OBJ = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(SUNVEC_SRC)))
SUNVEC_BIN = $(BIN_DIR)/sunvec

VECINFO_DIR = vecinfo
VECINFO_SRC = $(wildcard $(VECINFO_DIR)/*.c)
VECINFO_OBJ = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(VECINFO_SRC)))
VECINFO_BIN = $(BIN_DIR)/vecinfo

TELCOMPARSE_DIR = telcomparse
TELCOMPARSE_SRC = $(wildcard $(TELCOMPARSE_DIR)/*.c)
TELCOMPARSE_OBJ = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(TELCOMPARSE_SRC)))
TELCOMPARSE_BIN = $(BIN_DIR)/telcomparse

.PHONY: all common sunvec cspice vecinfo telcomparse clean mkdirs

all: common cspice sunvec vecinfo telcomparse

clean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)

common: $(COM_OBJ)

cspice: $(CSPICE_OBJ)

sunvec: common cspice $(SUNVEC_BIN)

vecinfo: common $(VECINFO_BIN)

telcomparse: common $(TELCOMPARSE_BIN)

$(OBJ_DIR)/$(COM_DIR)/%.c.o: $(COM_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/$(COM_DIR)
	$(CC) $(STRICTCFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/$(CSPICE_SRC_DIR)/%.c.o: $(CSPICE_SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/$(CSPICE_DIR)
	@mkdir -p $(OBJ_DIR)/$(CSPICE_SRC_DIR)
	$(CC) $(CFLAGS) -I$(CSPICE_INC_DIR) -c $< -o $@

$(OBJ_DIR)/$(SUNVEC_DIR)/%.c.o: $(SUNVEC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/$(SUNVEC_DIR)
	$(CC) $(STRICTCFLAGS) $(CFLAGS) -I$(COM_DIR) -I$(CSPICE_INC_DIR) -c $< -o $@

$(SUNVEC_BIN): $(SUNVEC_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) $(COM_OBJ) $(CSPICE_OBJ) $(SUNVEC_OBJ) -o $(SUNVEC_BIN)

$(OBJ_DIR)/$(VECINFO_DIR)/%.c.o: $(VECINFO_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/$(VECINFO_DIR)
	$(CC) $(STRICTCFLAGS) $(CFLAGS) -I$(COM_DIR) -c $< -o $@

$(VECINFO_BIN): $(VECINFO_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) $(COM_OBJ) $(VECINFO_OBJ) -o $(VECINFO_BIN)

$(OBJ_DIR)/$(TELCOMPARSE_DIR)/%.c.o: $(TELCOMPARSE_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/$(TELCOMPARSE_DIR)
	$(CC) $(STRICTCFLAGS) $(CFLAGS) -I$(COM_DIR) -c $< -o $@

$(TELCOMPARSE_BIN): $(TELCOMPARSE_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) $(COM_OBJ) $(TELCOMPARSE_OBJ) -o $(TELCOMPARSE_BIN)