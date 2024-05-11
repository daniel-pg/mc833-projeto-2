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

LD_FLAGS=\
-lsqlite3


.PHONY: all
all: build_folder server client-linux

build_folder:
	mkdir -p build
	cp server/MusicDatabase.db build/MusicDatabase.db
	cp -r server/music build

server: server.o
	$(CC) -o build/server build/server.o $(LD_FLAGS)

client-linux: client-linux.o
	$(CC) -o build/client-linux build/client-linux.o $(LD_FLAGS)

server.o: server/server.c
	$(CC) -o build/server.o server/server.c $(CC_FLAGS)

client-linux.o: client/client-linux.c
	$(CC) -o build/client-linux.o client/client-linux.c $(CC_FLAGS)

.PHONY: clean
clean:
	rm -rf ./build
