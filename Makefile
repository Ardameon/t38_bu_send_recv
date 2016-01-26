.PHONY: all clean

CC = gcc
STRIP = strip

OBJ_DIR = ./obj
BIN_DIR = ./bin
CUR_DIR = ./
OBJ += $(OBJ_DIR)/t38_transmitter.o $(OBJ_DIR)/msg_proc.o $(OBJ_DIR)/fax.o $(OBJ_DIR)/udptl.o

SPANDSP_DIR = spandsp/x86_64/deploy/usr/local

LIBS += $(shell pkg-config --libs-only-l $(SPANDSP_DIR)/lib/pkgconfig/spandsp.pc) 

INCLUDE = -I$(SPANDSP_DIR)/include
LDFLAGS = -L$(SPANDSP_DIR)/lib

CFLAGS += -Wall -O0 -g $(INCLUDE) -pthread
BIN += $(BIN_DIR)/fax_transmitter

all: striped

striped: $(BIN)
	cp $(BIN) $(BIN)_uns
	$(STRIP) $(BIN)

$(BIN): $(OBJ) $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $(BIN) $(LIBS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(BIN_DIR)

