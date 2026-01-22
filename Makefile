CFLAGS = -Wall -Wextra -pedantic -std=c23 

all: watchfd

watchfd:
	@gcc $(CFLAGS) $(shell pkg-config --cflags --libs glib-2.0 libnotify) src/main.c -o build/watchfd 
