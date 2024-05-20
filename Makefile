# C Compiler
CC=gcc

# Compiler flags
CC_FLAGS=\
-c              \
-g              \
-O3             \
-march=native   \
-std=gnu99      \
-pedantic       \
-Wall           \
-Werror=vla     \
-Wextra

SERVER_LD_FLAGS=\
-lsqlite3

CLIENT_LD_FLAGS :=

BUILD_DIR = build

.PHONY: all
all: setup server client-linux

setup:
	mkdir -p $(BUILD_DIR)
	cp server/MusicDatabase.db $(BUILD_DIR)/MusicDatabase.db
	cp -r server/music $(BUILD_DIR)

server: server.o
	$(CC) -o $(BUILD_DIR)/server $(BUILD_DIR)/server.o $(SERVER_LD_FLAGS)

client-linux: client-linux.o
	$(CC) -o $(BUILD_DIR)/client-linux $(BUILD_DIR)/client-linux.o $(CLIENT_LD_FLAGS)

server.o: server/server.c
	$(CC) -o $(BUILD_DIR)/server.o server/server.c $(CC_FLAGS)

client-linux.o: client/client-linux.c
	$(CC) -o $(BUILD_DIR)/client-linux.o client/client-linux.c $(CC_FLAGS)

.PHONY: clean
clean:
	rm -rf ./$(BUILD_DIR)
