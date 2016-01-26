.PHONY: all clean

CC = gcc
STRIP = strip

OBJ_DIR = ./obj
BIN_DIR = ./bin
CUR_DIR = ./
OBJ += $(OBJ_DIR)/t38_transmitter.o $(OBJ_DIR)/msg_proc.o 

CFLAGS += -Wall -O0 -g
BIN += $(BIN_DIR)/fax_transmitter

all: striped

striped: $(BIN)
	cp $(BIN) $(BIN)_uns
	$(STRIP) $(BIN)

$(BIN): $(OBJ) $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJ) -o $(BIN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(BIN_DIR)

