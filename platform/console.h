/* The console the game runs on: the SNES runtime (runtime/, snrt) behind the game's hardware calls
 * (game/hal.h, platform/hal.c), on the clock that gives the game the cartridge's time
 * (platform/clock.c). A program that runs the game (platform/main.c, test/golden.c) loads the ROM,
 * opens the clock, sets the multitap and calls console_run; it supplies the two functions below. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "snrt/snrt.h"
#include "snrt/dsp.h"

extern snrt machine;                    /* the console the game is running on */
extern snrt_dsp dsp;                    /* its sound chip, whose RAM is the sound CPU's (platform/clock.c) */

/* Game Speed (platform/clock.c): 1, Original, the default, the cartridge's time; 0, Smooth, the
 * game's passes 8 times as
 * fast (each pass in one vblank). Set before clock_open, or between frames. */
extern int original_speed;

/* the console powered on, the game's time at zero */
void clock_open(void);

/* the console powered on again, the game's time at zero and every count of the clock's with it */
void clock_reset(void);

/* runs the game from its reset (platform/hal.c); never returns */
void console_run(void);

/* from a frame's end (console_frame_done): the console powered on again as at the program's start
 * (work RAM cleared, the clock reset, the multitap set) and the game run from its reset; never
 * returns (online play starts and ends this way, every machine at the same point) */
void console_restart(int multitap);

/* the program's: after each frame (the frames so far; its picture is machine.picture, the pads for
 * the next go in machine.io.pad), and each sound sample (stereo, 32,040 a second) */
void console_frame_done(uint32_t frame);
void console_sample(int16_t l, int16_t r);

/* the clock's side of the game's hardware calls (platform/hal.c sends them here) */
void clock_write(uint16_t addr, uint8_t v);
uint8_t clock_read(uint16_t addr);
void clock_wait(void);
void clock_entry(uint32_t addr);        /* a routine's start (its cartridge address) */
void clock_mask(int on);                /* SetIRQ / IRQOff start (1) and return (0): the original's sei */
void clock_mark(uint32_t addr);         /* hal_cost_mark */

/* states, for online play's rollback (platform/snapstate.c): the clock's own (its time, its
 * levels, the sound CPU and chip) in clock_save(NULL) bytes; whether the frame ends inside the
 * game's wait for an interrupt (the only moments a state is taken: no routine is half run) */
size_t clock_save(uint8_t *out);
void clock_load(const uint8_t *in);
int clock_waiting(void);
/* the hal's (the stack reset's resume, the restart's multitap), and the top of the game's C stack
 * (console_run's frame: the game's frames are all below it) */
size_t hal_save(uint8_t *out);
void hal_load(const uint8_t *in);
extern char *console_stack_top;
