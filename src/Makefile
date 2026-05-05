CC      = gcc
CFLAGS  = -g -Wall -Werror -pedantic-errors
LDFLAGS =
LDLIBS  =

SERVER  = chatserver
CLIENT  = chatclientgui

SERVER_SRC = chatserver.c
CLIENT_SRC = chatclientgui.c

SERVER_OBJ = chatserver.o
CLIENT_OBJ = chatclientgui.o

.PHONY: all clean

all: $(SERVER) $(CLIENT)

# Build server
$(SERVER): $(SERVER_OBJ)
	$(CC) $(LDFLAGS) $< -o $@ $(LDLIBS)

# Build client (needs ncurses)
$(CLIENT): $(CLIENT_OBJ)
	$(CC) $(LDFLAGS) $< -o $@ -lncurses

# Compile object files (include util.h dependency)
%.o: %.c util.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(SERVER) $(CLIENT)
