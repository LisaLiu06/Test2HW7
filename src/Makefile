CC = gcc
CFLAGS = -Wall -Wextra -g

all: chatserver chatclientgui

chatserver: chatserver.c util.h
	$(CC) $(CFLAGS) chatserver.c -o chatserver

chatclientgui: chatclientgui.c util.h
	$(CC) $(CFLAGS) chatclientgui.c -o chatclientgui -lncurses

clean:
	rm -f chatserver chatclientgui
