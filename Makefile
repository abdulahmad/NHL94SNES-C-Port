# NHL '94 (SNES) in C: make builds build/snes94 (the game in a window) and build/golden
# (the game headless, for test/check.sh); make dist builds build/dist/snes94, one file with SDL2 in
# it. Needs gcc, make, awk, Python 3 and SDL2 (sdl2-config; Windows: an MSYS2 UCRT64 shell), and
# your own NHL '94 ROM: the first .smc (or .sfc) beside this Makefile, or make ROM=path. The sound
# CPU's program is made from it (tools/gen_spc.py): the cartridge's sound driver is not in this
# repository.
CC      ?= gcc
WARN    := -Wall -Wextra
CFLAGS  := -std=c11 $(WARN) -O2 -g
GAMEINC := -Igame -Iplatform -Iruntime/include
# game/ is instrumented: each routine's entry is an event the clock times (platform/hal.c, clock.c)
comma   := ,
empty   :=
space   := $(empty) $(empty)
GAMEH   := $(wildcard game/*.h)
RTH     := $(wildcard runtime/include/snrt/*.h)
INSTR   := -finstrument-functions -finstrument-functions-exclude-file-list=$(subst $(space),$(comma),$(GAMEH))
GAMEDEF := -DHeapFree=HeapFree_game   # (the routine's name is Windows' too, which SDL brings in)
SDLCF   := $(shell sdl2-config --cflags 2>/dev/null)
SDLLIB  := $(filter-out -mwindows,$(shell sdl2-config --libs 2>/dev/null))
SDLSTATIC := $(filter-out -mwindows,$(shell sdl2-config --static-libs 2>/dev/null))
ifeq ($(OS),Windows_NT)
EXE     := .exe
WINDRES ?= $(patsubst %gcc,%windres,$(CC))
ICONO   := build/obj/icon.o   # (the exe's icon, res/icon.ico)
# Explorer launch needs only SDL2.dll beside the exe (no libgcc_s / libwinpthread
# from PATH, which are often the 32-bit ones from a 95PC tree → 0xc000007b).
WINLIB  ?= -static-libgcc -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
SDL_DLL := $(shell sdl2-config --prefix 2>/dev/null)/bin/SDL2.dll
endif
WINLIB  ?=
ROM     ?= $(firstword $(wildcard *.smc *.sfc))
# the first Python 3 that runs (on Windows python3 may be the Store's stand-in, which only says so)
PYTHON  ?= $(firstword $(foreach p,python3 python,$(if $(shell $(p) -c "import sys; print(sys.version_info[0])" 2>/dev/null | grep -x 3),$(p))) python3)

GAME    := $(wildcard game/*.c)
RUNTIME := $(wildcard runtime/src/*.c)
GAMEO   := $(GAME:%.c=build/obj/%.o)
RTO     := $(RUNTIME:%.c=build/obj/%.o)
CONSOLEO := build/obj/platform/hal.o build/obj/platform/clock.o build/obj/fnsyms_gen.o build/obj/uncounted_gen.o
OVERBARO := build/obj/overbar/scaler.o build/obj/overbar/host.o build/obj/overbar/present.o \
	build/obj/overbar/menu.o build/obj/overbar/overlay.o build/obj/overbar/filemenu.o \
	build/obj/overbar/pulseman_menu.o
WINDOWO := build/obj/platform/main.o build/obj/platform/padsetup.o build/obj/platform/menu.o build/obj/platform/settings.o \
	build/obj/platform/overbar_client.o $(OVERBARO)

all: build/snes94$(EXE) build/golden$(EXE)

build/obj/game/%.o: game/%.c $(GAMEH)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INSTR) $(GAMEDEF) $(GAMEINC) -c $< -o $@
build/obj/platform/%.o: platform/%.c $(GAMEH) $(RTH) $(wildcard platform/*.h) platform/costs.inc
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(GAMEDEF) $(GAMEINC) -Iplatform/overbar/include $(if $(findstring main,$<)$(findstring overbar_client,$<),$(SDLCF)) -c $< -o $@
build/obj/overbar/%.o: platform/overbar/src/%.c platform/overbar/include/overbar/overbar.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iplatform/overbar/include $(SDLCF) -c $< -o $@
build/obj/overbar/pulseman_menu.o: platform/overbar/third_party/pulseman/menu.c platform/overbar/include/mdrt/menu.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iplatform/overbar/include -c $< -o $@
build/obj/runtime/%.o: runtime/%.c $(RTH)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iruntime/include -Ibuild -c $< -o $@
build/obj/runtime/src/spc.o: build/spc_program.inc
build/obj/test/%.o: test/%.c $(GAMEH) $(RTH) $(wildcard platform/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(GAMEINC) -c $< -o $@

# the sound CPU's programs as C, from the player's ROM
build/spc_program.inc: tools/gen_spc.py game/symbols_gen.h $(ROM)
	$(if $(ROM),,$(error No ROM: put your NHL '94 .smc beside the Makefile, or make ROM=path))
	@mkdir -p $(dir $@)
	$(PYTHON) tools/gen_spc.py "$(ROM)" game/symbols_gen.h > $@.tmp && mv $@.tmp $@

# the C routines and their cartridge addresses; the routines the clock does not count
build/fnsyms_gen.c: $(GAME) game/symbols_gen.h tools/gen_fnsyms.awk
	@mkdir -p $(dir $@)
	awk -f tools/gen_fnsyms.awk game/symbols_gen.h $(GAME) > $@
build/uncounted_gen.c: platform/uncounted.txt
	@mkdir -p $(dir $@)
	{ echo '/* Generated from platform/uncounted.txt: do not edit. */'; echo '#include <stdint.h>'; echo '#include "symbols_gen.h"'; \
	  echo 'const uint32_t uncounted[] = {'; grep -v '^#' $< | sed 's/.*/    SYM_&,/'; echo '};'; \
	  echo 'const int n_uncounted = sizeof uncounted / sizeof *uncounted;'; } > $@
build/obj/%_gen.o: build/%_gen.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(GAMEDEF) $(GAMEINC) -c $< -o $@

build/obj/icon.o: res/icon.rc res/icon.ico
	@mkdir -p $(dir $@)
	$(WINDRES) -I res $< -O coff -o $@
build/snes94$(EXE): $(GAMEO) $(CONSOLEO) $(RTO) $(WINDOWO) $(ICONO)
	$(CC) -o $@ $^ $(SDLLIB) $(WINLIB) -lm
	@if [ -n "$(SDL_DLL)" ] && [ -f "$(SDL_DLL)" ]; then cp -f "$(SDL_DLL)" build/SDL2.dll; fi
build/golden$(EXE): $(GAMEO) $(CONSOLEO) $(RTO) build/obj/test/golden.o
	$(CC) -o $@ $^ -lm
# one file to give a player: SDL2 and the compiler's runtime in it, nothing but the system's DLLs needed
dist: build/dist/snes94$(EXE)
build/dist/snes94$(EXE): $(GAMEO) $(CONSOLEO) $(RTO) $(WINDOWO) $(ICONO)
	@mkdir -p $(dir $@)
	$(CC) -static -s -o $@ $^ $(SDLSTATIC) -lm

clean:
	rm -rf build

.PHONY: all clean dist
