.RECIPEPREFIX := >

GBDKDIR ?= $(HOME)/.local/opt/gbdk/
LCC := $(GBDKDIR)bin/lcc
PNG2ASSET := $(GBDKDIR)bin/png2asset

EMU ?= sameboy
ROM := build/demo001.gb

ASSETS := \
	src/brickwall.c \
	src/floor.c \
	src/crate.c \
	src/robot.c

C_SOURCES := \
	src/main.c \
	src/brickwall.c \
	src/floor.c \
	src/crate.c \
	src/robot.c \
	src/tiny_font.c \
	src/window_text.c

.PHONY: all assets clean rebuild run check

all: $(ROM)

assets: $(ASSETS)

src/brickwall.c src/brickwall.h: assets/brickwall.png
>$(PNG2ASSET) assets/brickwall.png \
>	-tiles_only \
>	-keep_palette_order \
>	-o src/brickwall.c

src/floor.c src/floor.h: assets/floor.png
>$(PNG2ASSET) assets/floor.png \
>	-tiles_only \
>	-keep_palette_order \
>	-o src/floor.c

src/crate.c src/crate.h: assets/crate.png
>$(PNG2ASSET) assets/crate.png \
>	-tiles_only \
>	-keep_palette_order \
>	-o src/crate.c

src/robot.c src/robot.h: assets/robot.png
>$(PNG2ASSET) assets/robot.png \
>	-spr8x16 \
>	-sw 16 \
>	-sh 16 \
>	-keep_palette_order \
>	-o src/robot.c

$(ROM): $(C_SOURCES)
>mkdir -p build
>$(LCC) -o $(ROM) $(C_SOURCES)

run: $(ROM)
>$(EMU) $(ROM)

rebuild: clean all

check:
>@echo "GBDKDIR=$(GBDKDIR)"
>@test -x "$(LCC)" || (echo "Missing lcc: $(LCC)" && exit 1)
>@test -x "$(PNG2ASSET)" || (echo "Missing png2asset: $(PNG2ASSET)" && exit 1)
>@test -f assets/brickwall.png || (echo "Missing assets/brickwall.png" && exit 1)
>@test -f assets/floor.png || (echo "Missing assets/floor.png" && exit 1)
>@test -f assets/crate.png || (echo "Missing assets/crate.png" && exit 1)
>@test -f assets/robot.png || (echo "Missing assets/robot.png" && exit 1)
>@echo "Toolchain and assets look present."

clean:
>rm -rf build
>rm -f src/brickwall.c src/brickwall.h
>rm -f src/floor.c src/floor.h
>rm -f src/crate.c src/crate.h
>rm -f src/robot.c src/robot.h