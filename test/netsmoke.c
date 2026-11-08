/* Online play headless, for test/netsmoke.sh: the game with no window on platform/netplay.c, its
 * pads random (each machine its own seed), a session on the network after some frames of a local
 * game (so the machines differ before it: the session's power on must make them the same again).
 * At step N it prints the step, the hash of the work RAM, the sound CPU's RAM and the picture, and
 * the settings the session runs with; then it plays on a little (so the others reach N too), says
 * goodbye and exits.
 *   netsmoke ROM (--host PORT | --join ADDR:PORT) [--steps N] [--pre FRAMES] [--seed S] [--here H]
 *            [--all A] [--delay D] [--smooth] [--lag MS] [--loss PCT]
 * (--lag, --loss: this machine's sends delayed by MS, jittered by MS / 2, PCT per cent lost; with
 * rollback the frames run on, the step's hash printed is its last, once every machine is past it) */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"
#include "mem.h"
#include "rom.h"
#include "console.h"
#include "netplay.h"

static ob_online o;
static uint32_t steps = 600, pre = 90, seed = 1, after = 120;
static int started, printed;
static uint32_t frames, rng, t0;
static uint16_t held[NETPLAY_PADS];
static int lag, loss;
static char last[200];             /* the line for step N, as last played (a replay's replaces a guess's) */
static uint32_t replays;

static uint32_t ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void nap(void)
{
    struct timespec ts = { 0, 1000000 };
    nanosleep(&ts, NULL);
    if (ms() - t0 > 120000) { fprintf(stderr, "netsmoke: no end after 120 s\n"); exit(1); }
}

static uint64_t fnv(const void *p, size_t n, uint64_t h)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001B3ull;
    return h;
}

void console_sample(int16_t l, int16_t r) { (void)l; (void)r; }

void console_frame_done(uint32_t frame)
{
    (void)frame;
    frames++;
    netplay_frame(ms());
    if (!started && frames >= pre) {
        char msg[128] = "";
        if (netplay_start(&o, 0, msg, sizeof msg) != 0) { fprintf(stderr, "netsmoke: %s\n", msg); exit(1); }
        if (lag || loss) netplay_simulate(lag, lag / 2, loss);
        started = 1;
    }
    if (started && !netplay_on()) {             /* (the other said goodbye after the last step: done) */
        char line[128];
        netplay_status(line, sizeof line);
        fprintf(stderr, "netsmoke: the session ended: %s\n", line);
        if (printed) { printf("%s\n", last); fflush(stdout); }
        exit(printed ? 0 : 1);
    }
    for (int p = 0; p < NETPLAY_PADS; p++) {      /* (the buttons change now and then, as a player's do) */
        rng = rng * 1664525u + 1013904223u;
        if ((rng >> 24) < 24) held[p] = (uint16_t)((rng >> 4) & 0xFFF0);
    }
    uint16_t pad[NETPLAY_PADS];
    if (netplay_pads(held, pad, nap)) {
        memcpy(machine.io.pad, pad, sizeof pad);
        uint32_t s = netplay_steps();
        replays += netplay_replaying();
        if (s == steps) {
            uint64_t h = fnv(WRAM_BYTES, 0x20000, 0xCBF29CE484222325ull);
            h = fnv(dsp.ram, sizeof dsp.ram, h);
            h = fnv(machine.picture, sizeof machine.picture, h);
            char line[128];
            netplay_status(line, sizeof line);
            snprintf(last, sizeof last, "step %u hash %016llx speed %s multitap %d (%s)", (unsigned)s, (unsigned long long)h,
                     original_speed ? "original" : "smooth", machine.io.multitap, line);
            printed = 1;
        }
        if (s >= steps + after) {
            printf("%s\n", last);
            fprintf(stderr, "netsmoke: %u frames played again\n", (unsigned)replays);
            netplay_leave();
            exit(0);
        }
    } else memcpy(machine.io.pad, held, sizeof held);
}

int main(int argc, char **argv)
{
    const char *rom = NULL;
    memset(&o, 0, sizeof o);
    o.port = OB_NET_PORT_DEFAULT;
    o.delay = 2;
    o.here = 1;
    o.all = 2;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--host") && i + 1 < argc) { o.role = 0; o.port = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--join") && i + 1 < argc) { o.role = 1; snprintf(o.addr, sizeof o.addr, "%s", argv[++i]); }
        else if (!strcmp(argv[i], "--steps") && i + 1 < argc) steps = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pre") && i + 1 < argc) pre = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--here") && i + 1 < argc) o.here = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--all") && i + 1 < argc) o.all = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--delay") && i + 1 < argc) o.delay = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--smooth")) original_speed = 0;
        else if (!strcmp(argv[i], "--lag") && i + 1 < argc) lag = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--loss") && i + 1 < argc) loss = atoi(argv[++i]);
        else rom = argv[i];
    }
    if (!rom) { fprintf(stderr, "usage: netsmoke ROM (--host PORT | --join ADDR:PORT) [options]\n"); return 2; }
    if (!rom_load(rom)) { fprintf(stderr, "cannot load %s\n", rom); return 2; }
    snprintf(o.name, sizeof o.name, "%s", o.role ? "guest" : "host");
    rng = seed;
    t0 = ms();
    clock_open();
    machine.io.multitap = 0;
    console_run();
}
