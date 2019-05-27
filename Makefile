SRC = cave.c gfx.c
CFLAGS = -g
LFLAGS = -lSDL2 -o cave

all:
	@echo cc $(SRC)
	@gcc $(SRC) $(CFLAGS) $(LFLAGS)

