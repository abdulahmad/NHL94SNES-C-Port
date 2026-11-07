/* game/hal.h on the SNES runtime: the registers go to the runtime (snrt) through the clock
 * (platform/clock.c), the stack reset drops the C frames, hooks outside snes.c's own dispatch find
 * their routine by address, and DMA reaches the game's memory. game/ is compiled with
 * -finstrument-functions: each routine's entry is an event the clock times, and a moment it can
 * place an interrupt at. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "mem.h"
#include "console.h"
#include "fnsyms.h"

#define NOI __attribute__((no_instrument_function))

snrt machine;

void io_w8(uint16_t addr, uint8_t v) { clock_write(addr, v); }
uint8_t io_r8(uint16_t addr) { return clock_read(addr); }
void hal_wait_interrupt(void) { clock_wait(); }
void hal_cost_mark(uint32_t addr) { clock_mark(addr); }

/* the stack reset: back to console_run, which goes on with resume */
static jmp_buf top;
static void (*resume_fn)(void);
void hal_stack_reset(void (*resume)(void))
{
    resume_fn = resume;
    longjmp(top, 1);
}

/* power on again: as the program starts (the work RAM zero, as it is before the game runs) */
static uint8_t restart_tap;
static void power_on(void)
{
    clock_reset();
    memset(&g, 0, sizeof g);
    machine.io.multitap = restart_tap;
    Start();
}
void console_restart(int multitap)
{
    restart_tap = (uint8_t)(multitap != 0);
    hal_stack_reset(power_on);
}

static int sorted_fn, sorted_addr;
static FnSym by_addr[4096];

NOI static int cmp_fn(const void *a, const void *b)
{
    uintptr_t x = (uintptr_t)((const FnSym *)a)->fn, y = (uintptr_t)((const FnSym *)b)->fn;
    return x < y ? -1 : x > y;
}
NOI static int cmp_addr(const void *a, const void *b)
{
    uint32_t x = ((const FnSym *)a)->addr, y = ((const FnSym *)b)->addr;
    return x < y ? -1 : x > y;
}

/* the routine at a cartridge address (NULL: none) */
NOI static void (*routine_at(uint32_t addr))(void)
{
    if (!sorted_addr) {
        memcpy(by_addr, fn_syms, sizeof *fn_syms * (size_t)n_fn_syms);
        qsort(by_addr, (size_t)n_fn_syms, sizeof *by_addr, cmp_addr);
        sorted_addr = 1;
    }
    FnSym key = { NULL, addr }, *f = bsearch(&key, by_addr, (size_t)n_fn_syms, sizeof *by_addr, cmp_addr);
    return f ? f->fn : NULL;
}

/* a C function's cartridge address as the clock counts routines (0: not a routine, or one left
 * out: platform/uncounted.txt) */
static FnSym by_fn[4096];
extern const uint32_t uncounted[];
extern const int n_uncounted;
NOI static uint32_t address_of(void *fn)
{
    if (!sorted_fn) {
        memcpy(by_fn, fn_syms, sizeof *fn_syms * (size_t)n_fn_syms);
        for (int i = 0; i < n_fn_syms; i++)
            for (int k = 0; k < n_uncounted; k++) if (by_fn[i].addr == uncounted[k]) by_fn[i].addr = 0;
        qsort(by_fn, (size_t)n_fn_syms, sizeof *by_fn, cmp_fn);
        sorted_fn = 1;
    }
    FnSym key = { (void (*)(void))fn, 0 }, *f = bsearch(&key, by_fn, (size_t)n_fn_syms, sizeof *by_fn, cmp_fn);
    return f ? f->addr : 0;
}

NOI void __cyg_profile_func_enter(void *fn, void *site);
NOI void __cyg_profile_func_exit(void *fn, void *site);
void __cyg_profile_func_enter(void *fn, void *site)
{
    (void)site;
    uint32_t a = address_of(fn);
    if (a) clock_entry(a);
    if (fn == (void *)SetIRQ || fn == (void *)IRQOff) clock_mask(1);
}
void __cyg_profile_func_exit(void *fn, void *site)
{
    (void)site;
    if (fn == (void *)SetIRQ || fn == (void *)IRQOff) clock_mask(0);
}

/* a hook snes.c's call_long does not know: the routine at its address; the RAM trampoline
 * jmlvector ($00:1D59, a jml) goes to the address it holds */
static void call_by_address(uint32_t addr)
{
    if ((addr & 0x7FFFFF) == RAM_OFF_jmlvector) addr = r16(RAM_OFF_jmlvector + 1) | (uint32_t)r8(RAM_OFF_jmlvector + 3) << 16;
    if ((addr >> 16 & 0xFF) < 0x40) addr |= 0x800000;
    void (*f)(void) = routine_at(addr);
    if (!f) { fprintf(stderr, "a hook at $%06X: no routine there\n", addr); exit(3); }
    f();
}

/* the A bus as DMA and the WRAM port reach it */
static uint8_t read_a(void *user, uint32_t addr) { (void)user; return m8(addr); }
static void write_a(void *user, uint32_t addr, uint8_t v) { (void)user; if (is_wram(addr)) w8(wram_off(addr), v); }

void console_run(void)
{
    machine.read_a = read_a;
    machine.write_a = write_a;
    call_long_other = call_by_address;
    if (setjmp(top)) resume_fn();
    else Start();
    fprintf(stderr, "the game returned from its top level\n");
    exit(3);
}
