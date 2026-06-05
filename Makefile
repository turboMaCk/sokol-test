CC := gcc
GAME_OUT := game
SHADER_SRCS := $(wildcard shaders/*.glsl)
SHADERS_H := $(SHADER_SRCS:.glsl=.h)
PLATFORM_C_FLAGS := $(shell pkg-config --cflags --libs x11 xcursor xi xrandr xext xfixes gl)
C_FLAGS := -std=c99 -DSOKOL_GLCORE -Wall -Wextra -I$(PWD)/include -lm -g $(PLATFORM_C_FLAGS)

.PHONY: all
all: game

vendor.o: $(wildcard include/*.h) vendor.c
	$(CC) $(C_FLAGS) -c vendor.c -o $@

$(GAME_OUT): main.c vendor.o $(wildcard engine/*.h) $(SHADERS_H)
	$(CC) $(C_FLAGS) main.c vendor.o -o $@

shaders/%.h: shaders/%.glsl Makefile
	sokol-shdc --input  $< --output $@ --slang glsl430

.PHONY: clean
clean:
	$(RM) $(GAME_OUT) $(SHADERS_H) vendor.o
