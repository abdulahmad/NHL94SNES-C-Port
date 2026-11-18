/* The game's time. The C has no clock of its own, so the console's time comes from a cost model
 * of the cartridge's CPU: a pass of the game loop takes 2 vblanks, sometimes 3, as the 65816 does,
 * and the game scales its moves by the vblanks as always.
 *
 * Time is master cycles since power on (line 225, cycle 0). Each event of the game's work, a routine
 * entry, a register access or a cost mark, moves the beam on by what the cartridge takes from the
 * event before it at the same depth in interrupt handlers to this one: platform/costs.inc, fitted
 * from traces of the cartridge in an emulator (the mean of the pair's segments, handlers and DMA left
 * out). An interrupt whose time comes inside that step is taken there: the beam stops at it, the
 * handler runs (its own events moving the beam on), and the rest of the step comes after. The NMI
 * comes at line 225, cycle 90 when enabled; the IRQ when the H/V timer fires, unless masked: inside
 * the IRQ handler, and in SetIRQ and IRQOff (the original's sei; the NMI handler takes IRQs,
 * VBlank's cli). A wait (hal_wait_interrupt) runs the console to the next interrupt; the step
 * after it is the cost from the wait loop's exit (WAIT) to the next event. A DMA adds its bytes'
 * time (8 cycles a byte) at its $420B write.
 *
 * Sound: the sound CPU (snrt/spc.h) runs to the game's time at each port access and each frame's
 * end. Command $10 (SPCStreamNext) is a plain copy: the original keeps step with the sound CPU's
 * loop, a byte each pass and no handshake, which a cost model cannot; the sound CPU is held at the
 * loop, each byte goes straight to its RAM, the count it echoes answers at once, and the loop ends
 * as on the cartridge (its count at the bytes sent).
 *
 * Game Speed Smooth (original_speed 0): inside a pass of the game loop, from DoGameFrame to the next wait outside
 * the interrupt handlers, each step costs an eighth of the cartridge's (rounded up), as if the CPU
 * were 8 times as fast; a pass then fits in one vblank, and the game runs at the vblanks' rate. The
 * handlers and the waits keep the cartridge's time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "mem.h"
#include "console.h"
#include "snrt/spc.h"

#define FRAME ((int64_t)SNRT_LINES * SNRT_LINE_CYCLES)
#define NMI_AT 90                 /* line 225's cycle where the NMI comes */
#define SPC_PER_FRAME_X10 170599  /* the sound CPU's cycles a frame times 10 (32,040 samples a second of 32 cycles) */
#define NMI_K 0x02000000u         /* the events' keys: a routine's address, */
#define IRQ_K 0x02000001u         /* 0x01000000 | 1 << 16 for a read | the register, an interrupt's */
#define RTI_K 0x02000002u         /* start and return, a wait */
#define WAIT_K 0x02000003u
#define MARK_K 0x03000000u         /* (a cost mark: | its address) */
#define ACCESS_K(read, reg) (0x01000000u | (uint32_t)(read) << 16 | (reg))
#define STREAM_LOOP 0x07DF        /* the sound driver's command $10 loop (.nextbyte) and its end */
#define STREAM_END 0x07EA

typedef struct { uint32_t prev, next, ctx, cycles; } cost;
static const cost costs[] = {
#include "costs.inc"
};
static const size_t n_costs = sizeof costs / sizeof *costs;

static int64_t now, beam_at, nmi_next = NMI_AT;
static struct { uint32_t prev, ctx; int waited; } lv[16];   /* per depth: the last event, the last routine entered */
static int depth, masked, in_irq;
static int in_pass;                          /* a game loop's pass: from DoGameFrame to the next wait out of handlers */
int original_speed = 1;
static uint32_t frame;                       /* frames ended */

snrt_dsp dsp;
static snrt_spc spc;
static int stream, stream_armed;             /* command $10: the bytes copied (stream: copying) */
static int waiting;                          /* inside clock_wait's move to the next interrupt (a resumable point) */

static int before(const cost *c, uint32_t prev, uint32_t next, uint32_t ctx)
{
    return c->prev != prev ? c->prev < prev : c->next != next ? c->next < next : c->ctx < ctx;
}
static const cost *find(uint32_t prev, uint32_t next, uint32_t ctx)
{
    size_t lo = 0, hi = n_costs;
    while (lo < hi) {
        size_t m = (lo + hi) / 2;
        if (before(&costs[m], prev, next, ctx)) lo = m + 1;
        else hi = m;
    }
    const cost *c = &costs[lo];
    return lo < n_costs && c->prev == prev && c->next == next && c->ctx == ctx ? c : NULL;
}
/* the step's cost in its routine's context, else the pair's in any (100 cycles for a pair the
 * traces never had); an eighth of it in a pass at Smooth speed */
static uint32_t cost_of(uint32_t prev, uint32_t next, uint32_t ctx)
{
    const cost *c = find(prev, next, ctx);
    if (!c) c = find(prev, next, 0);
    uint32_t cycles = c ? c->cycles : 100;
    return original_speed || !in_pass ? cycles : (cycles + 7) / 8;
}

/* the beam to now (a frame at most a step: snrt_beam goes to the next time the beam is there) */
static void beam_to(int64_t t)
{
    int64_t f = t % FRAME;
    snrt_beam(&machine, (int)((225 + f / SNRT_LINE_CYCLES) % SNRT_LINES), (int)(f % SNRT_LINE_CYCLES));
}
static void set_now(int64_t t)
{
    while (t - beam_at > FRAME / 2) { beam_at += FRAME / 2; beam_to(beam_at); }
    beam_at = now = t;
    beam_to(t);
}

static int irq_on(void) { return !masked && !in_irq && (machine.io.nmitimen & 0x30); }

/* the first interrupt due by target (now if one is waiting), or -1 */
static int64_t next_interrupt(int64_t target, int *irq)
{
    int64_t best = -1;
    while (nmi_next <= target && !(machine.io.nmitimen & 0x80)) nmi_next += FRAME;   /* (off as it passes: none) */
    if (nmi_next <= target) { best = nmi_next < now ? now : nmi_next; *irq = 0; }
    if (!irq_on()) return best;
    if (machine.io.irq_flag) { *irq = 1; return now; }
    int64_t line_start = now - (now % FRAME) % SNRT_LINE_CYCLES;
    for (int64_t ls = line_start; ls <= target && (best < 0 || ls <= best); ls += SNRT_LINE_CYCLES) {
        int line = (int)((225 + (ls % FRAME) / SNRT_LINE_CYCLES) % SNRT_LINES);
        int c = snrt_io_irq_cycle(&machine.io, line);
        if (c < 0) continue;
        int64_t t = ls + c;
        if (t > now && t <= target && (best < 0 || t < best)) { best = t; *irq = 1; }
    }
    return best;
}

static void advance(int64_t cycles);

static void event(uint32_t key)
{
    uint32_t prev = lv[depth].waited ? WAIT_K : lv[depth].prev, ctx = lv[depth].ctx;
    lv[depth].prev = key;
    lv[depth].waited = 0;
    if (key < 0x01000000) lv[depth].ctx = key;
    advance(cost_of(prev, key, ctx));
}

static void take(int irq)
{
    if (irq) in_irq++;
    else nmi_next = (now / FRAME + 1) * FRAME + NMI_AT;
    depth++;
    lv[depth].prev = lv[depth].ctx = irq ? IRQ_K : NMI_K;
    lv[depth].waited = 0;
    if (irq) IRQ();
    else NMI();
    event(RTI_K);
    depth--;
    if (irq) in_irq--;
}

static void advance(int64_t cycles)
{
    int64_t target = now + cycles;
    for (;;) {
        int irq = 0;
        int64_t t = next_interrupt(target, &irq);
        if (t < 0) { set_now(target); return; }
        set_now(t);
        int64_t rest = target - now;
        take(irq);
        target = now + rest;
    }
}

/* ---- the sound CPU ---- */

static void spc_catch_up(void)
{
    if (!stream) snrt_spc_run(&spc, (uint64_t)now * SPC_PER_FRAME_X10 / (FRAME * 10));   /* (integers: the same on every host) */
}

static uint16_t free_ptr(void) { return (uint16_t)(dsp.ram[3] | dsp.ram[4] << 8); }

static void stream_byte(uint8_t v)
{
    if (!stream) {   /* (the first byte: the sound CPU to the loop's start, its count 0) */
        for (int n = 0; n < 64 && !(spc.pc == STREAM_LOOP && spc.cyc == 0); n++) snrt_spc_run(&spc, spc.clock + 1);
        spc.pc = STREAM_LOOP; spc.cyc = 0; spc.y = 0;
    }
    stream++;
    dsp.ram[(uint16_t)(free_ptr() + stream)] = v;
    spc.port_in[2] = v;
}

/* the loop ends as on the cartridge: the count at the bytes sent (the free pointer moves by it) */
static void stream_end(void)
{
    uint8_t y = (uint8_t)stream;
    dsp.ram[0xF5] = y;
    spc.y = y; spc.a = 0; spc.z = 1; spc.n = 0;
    spc.pc = STREAM_END; spc.cyc = 0;
    stream = 0;
}

static void sample(void *user, int16_t l, int16_t r)
{
    (void)user;
    console_sample(l, r);
}

/* ---- the game's hardware calls ---- */

void clock_write(uint16_t addr, uint8_t v)
{
    event(ACCESS_K(0, addr));
    if (addr >= 0x2140 && addr <= 0x2143) {
        int port = addr & 3;
        if (port == 2 && stream_armed && (stream || dsp.ram[0xF4] == 0x10)) { stream_byte(v); return; }
        if (port == 0 && stream) {   /* (the end: the port first, so the held sound CPU finds it when it catches up) */
            stream_end();
            snrt_spc_port_write(&spc, port, v);
        }
        spc_catch_up();
        snrt_spc_port_write(&spc, port, v);
        if (port == 0) stream_armed = v == 0x10 && dsp.ram[0xF4] == 0;   /* (command $10 to the idle driver: not an upload's count reaching $10) */
        return;
    }
    int64_t dma = 0;
    if (addr == 0x420B)
        for (int c = 0; c < 8; c++) {
            if (!(v >> c & 1)) continue;
            uint16_t n = (uint16_t)(snrt_dma_read(&machine.dma, (uint16_t)(0x4305 + 16 * c)) | snrt_dma_read(&machine.dma, (uint16_t)(0x4306 + 16 * c)) << 8);
            dma += 8 * (n ? n : 0x10000) + 8;
        }
    int nmi_was = machine.io.nmitimen & 0x80;
    snrt_write(&machine, addr, v);
    if (addr == 0x4200 && !nmi_was && (v & 0x80) && machine.io.nmi_flag && machine.line >= 225) nmi_next = now;   /* (enabled in the vertical blank: at once) */
    if (dma) set_now(now + dma + 8);
}

uint8_t clock_read(uint16_t addr)
{
    event(ACCESS_K(1, addr));
    if (addr >= 0x2140 && addr <= 0x2143) {
        if (stream && addr == 0x2141) return (uint8_t)stream;
        spc_catch_up();
        return snrt_spc_port_read(&spc, addr & 3);
    }
    return snrt_read(&machine, addr, -1);
}

void clock_wait(void)
{
    int irq = 0;
    int64_t t = next_interrupt(now + 4 * FRAME, &irq);
    if (t < 0) { fprintf(stderr, "frame %u: a wait for an interrupt, but none is on\n", frame); exit(3); }
    lv[depth].waited = 1;
    if (!depth) in_pass = 0;
    waiting = 1;
    set_now(t);
    waiting = 0;
    take(irq);
}

void clock_entry(uint32_t addr)
{
    event(addr);
    if (addr == SYM_DoGameFrame && !depth) in_pass = 1;
}
void clock_mask(int on) { masked += on ? 1 : -1; }
void clock_mark(uint32_t addr) { event(MARK_K | (addr & 0xFFFFFF)); }

/* ---- each frame's end ---- */

static void frame_end(snrt *m, void *user)
{
    (void)m; (void)user;
    frame++;
    spc_catch_up();
    snrt_spc_sync_dsp(&spc);
    console_frame_done(frame);
}

/* power on again (online play's restart): the clock's time and its counts back to their start too */
void clock_reset(void)
{
    now = beam_at = 0;
    nmi_next = NMI_AT;
    memset(lv, 0, sizeof lv);
    depth = masked = in_irq = in_pass = 0;
    frame = 0;
    stream = stream_armed = 0;
    clock_open();
}

void clock_open(void)
{
    machine.frame_done = frame_end;
    snrt_power(&machine);
    dsp.sample = sample;
    snrt_dsp_power(&dsp);
    snrt_spc_power(&spc, &dsp);
}

/* ---- states (online play's rollback: platform/snapstate.c) ---- */

int clock_waiting(void) { return waiting; }

#define PART(x) { &(x), sizeof (x) }
static const struct { void *p; size_t n; } parts[] = {
    PART(now), PART(beam_at), PART(nmi_next), PART(lv), PART(depth), PART(masked), PART(in_irq), PART(in_pass),
    PART(frame), PART(spc), PART(dsp), PART(stream), PART(stream_armed), PART(waiting)
};

size_t clock_save(uint8_t *out)
{
    size_t at = 0;
    for (size_t i = 0; i < sizeof parts / sizeof *parts; i++) {
        if (out) memcpy(out + at, parts[i].p, parts[i].n);
        at += parts[i].n;
    }
    return at;
}

void clock_load(const uint8_t *in)
{
    size_t at = 0;
    for (size_t i = 0; i < sizeof parts / sizeof *parts; i++) { memcpy(parts[i].p, in + at, parts[i].n); at += parts[i].n; }
}
