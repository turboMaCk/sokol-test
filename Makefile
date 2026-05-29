CC:=gcc

.PHONY: all
all: game

game: main.c $(wildcard include/*.h) $(wildcard engine/*.h)
	$(CC) -DSOKOL_GLCORE -Wall -I$(PWD)/include main.c -o game -lm $(shell pkg-config --cflags --libs x11 xcursor xi xrandr xext xfixes gl)

.PHONY: clean
clean:
	$(RM) game
