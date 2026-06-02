CC := gcc
GAME_OUT := game
SHADER_SRCS := $(wildcard shaders/*.glsl)
SHADERS_H := $(SHADER_SRCS:.glsl=.h)
PLATFORM_C_FLAGS := $(shell pkg-config --cflags --libs x11 xcursor xi xrandr xext xfixes gl)
C_FLAGS := -DSOKOL_GLCORE -Wall -I$(PWD)/include -lm -g $(PLATFORM_C_FLAGS)

.PHONY: all
all: game

game: main.c $(wildcard include/*.h) $(wildcard engine/*.h) $(SHADERS_H)
	$(CC) $(C_FLAGS) main.c -o $(GAME_OUT)

shaders/%.h: shaders/%.glsl Makefile
	sokol-shdc --input  $< --output $@ --slang glsl430

.PHONY: clean
clean:
	$(RM) $(GAME_OUT) $(SHADERS_H)
