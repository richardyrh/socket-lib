CC := g++
CFLAGS := -Wall -shared -fPIC

SRC_DIR := .
LIB_DIR := ${RISCV}/lib
INC_DIR := ${RISCV}/include/socketlib

SRC_FILES := $(wildcard $(SRC_DIR)/*.cc)
HEADERS := $(wildcard $(SRC_DIR)/*.h)

TARGET := libsocketlib.so

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC_FILES)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

install: all
	mkdir -p $(LIB_DIR)
	mkdir -p $(INC_DIR)
	cp $(TARGET) $(LIB_DIR)/
	cp $(HEADERS) $(INC_DIR)/

rv64: $(SRC_FILES)
	riscv64-unknown-linux-gnu-g++ -Wall -static -c -o libsocketlibmmio.o $^
	ar rvs libsocketlibmmio.a libsocketlibmmio.o
	mkdir -p $(LIB_DIR)/rv64
	cp libsocketlibmmio.a $(LIB_DIR)/rv64/

rv32: $(SRC_FILES)
	riscv32-unknown-linux-gnu-g++ -Wall -static -c -o libsocketlibmmio.o $^
	ar rvs libsocketlibmmio.a libsocketlibmmio.o
	mkdir -p $(LIB_DIR)/rv32
	cp libsocketlibmmio.a $(LIB_DIR)/rv32/
