/* Online play on Overbar's netplay core with rollback (netplay.h). */
#include "netplay.h"
#include "snapstate.h"

#include <stdio.h>
#include <string.h>
#include "game.h"
#include "mem.h"
#include "rom.h"
#include "console.h"
#include "build_id.h"           /* BUILD_ID: git describe of this build (build/, made by the Makefile) */

#define CHECK_EVERY 60          /* steps between the hash checks */
#define BANNER_MS 6000          /* how long the line saying why a session ended stays */
#define SETTINGS_VERSION 1
#define ROLLBACK 12             /* frames one rollback plays again, at most */

static ob_net *net;
static int playing;             /* the session's console has been powered on: its steps are the frames */
static int restart;             /* the session ended while playing: the local game again at the frame's end */
static int local_tap, local_speed;
static char why[128];           /* why the last session ended */
static uint32_t why_at, now;
static char banner[160];

/* the cartridge's CRC-32 (the identity's "rom:") */
static uint32_t rom_crc(void)
{
    static uint32_t crc;
    static int done;
    if (done) return crc;
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < rom_size; i++) {
        c ^= rom_cart[i];
        for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1)));
    }
    crc = ~c;
    done = 1;
    return crc;
}

/* the simulation's hash: the work RAM, then the sound CPU's RAM */
static uint64_t sim_hash(void)
{
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < 0x20000; i++) h = (h ^ WRAM_BYTES[i]) * 0x100000001B3ull;
    for (size_t i = 0; i < sizeof dsp.ram; i++) h = (h ^ dsp.ram[i]) * 0x100000001B3ull;
    return h;
}

static void ended(const char *reason)
{
    int was = playing;
    snprintf(why, sizeof why, "%s", reason && reason[0] ? reason : "the session ended");
    why_at = now;
    ob_net_free(net);
    net = NULL;
    playing = 0;
    original_speed = local_speed;
    if (was) restart = 1;
}

int netplay_start(const ob_online *o, int local_multitap, char *msg, int msg_size)
{
    char identity[256];
    /* the host's Game Speed and multitap, which the guests take: the multitap is in when the host's
     * game has more than two players (as at power on, players 3-5 need it; two play without) */
    uint8_t settings[3] = { SETTINGS_VERSION, (uint8_t)(original_speed != 0), (uint8_t)(o->all > 2) };
    if (net) { snprintf(msg, (size_t)msg_size, "Already in a session: Leave first"); return -1; }
    /* what must be the same on every machine and cannot be taken from the host: the build (the C and
     * the cost model), the cartridge, the pads and these settings' layout */
    snprintf(identity, sizeof identity, "NHL94SNES-C-Port %s rom:%08x pads:%d set:%d", BUILD_ID, (unsigned)rom_crc(),
             NETPLAY_PADS, SETTINGS_VERSION);
    static const ob_net_states states = { NULL, snap_save, snap_load };
    ob_net_game gm = { identity, NETPLAY_PADS, settings, sizeof settings, &states, ROLLBACK };
    net = ob_net_new(o, &gm, msg, msg_size);
    if (!net) return -1;
    local_tap = local_multitap;
    local_speed = original_speed;
    playing = 0;
    restart = 0;
    why[0] = 0;
    return 0;
}

void netplay_leave(void)
{
    if (!net) return;
    ob_net_quit(net);
    ended("you left");
}

int netplay_status(char *line, int line_size)
{
    if (net) { ob_net_status(net, line, line_size); return 1; }
    snprintf(line, (size_t)line_size, "%s", why);
    return 0;
}

int netplay_on(void) { return net != NULL; }
int netplay_playing(void) { return net && playing; }
uint32_t netplay_steps(void) { return net && playing ? ob_net_step_count(net) : 0; }

const char *netplay_banner(uint32_t now_ms)
{
    char line[128];
    if (net && !playing) {
        ob_net_status(net, line, sizeof line);
        snprintf(banner, sizeof banner, "ONLINE: %s", line[0] ? line : "connecting");
        return banner;
    }
    if (!net && why[0] && now_ms - why_at < BANNER_MS) {
        snprintf(banner, sizeof banner, "ONLINE ENDED: %s", why);
        return banner;
    }
    return NULL;
}

void netplay_frame(uint32_t now_ms)
{
    now = now_ms;
    if (restart) { restart = 0; console_restart(local_tap); }
    if (!net) return;
    int st = ob_net_poll(net);
    if (st == OB_NET_ENDED) {
        ended(ob_net_why(net));
        if (restart) { restart = 0; console_restart(local_tap); }
        return;
    }
    if (st == OB_NET_GO && !playing) {      /* everyone is in: step 0 from power on, the host's settings */
        size_t n = 0;
        const uint8_t *s = ob_net_settings(net, &n);
        int tap = 0;
        if (s && n >= 3 && s[0] == SETTINGS_VERSION) { original_speed = s[1] != 0; tap = s[2] != 0; }
        playing = 1;
        console_restart(tap);
    }
}

void netplay_simulate(int delay_ms, int jitter_ms, int loss_pct) { if (net) ob_net_simulate(net, delay_ms, jitter_ms, loss_pct, 0); }

int netplay_replaying(void) { return net && playing && ob_net_replaying(net); }

/* where a state comes back to (snapstate.h): this frame, just before the core is asked */
static void *jb[5];

int netplay_pads(const uint16_t *local, uint16_t pad[NETPLAY_PADS], void (*wait)(void))
{
    uint16_t all[OB_NET_MAX_PADS];
    const uint16_t *volatile lp = local;
    uint16_t *volatile pp = pad;
    void (*volatile wp)(void) = wait;
    if (!net || !playing) return 0;
    __builtin_setjmp(jb);                   /* (a rollback comes back here, the frames as they were then) */
    snap_mark(jb, snap_sp());
    for (;;) {
        int r = ob_net_frame(net, clock_waiting(), lp, all);
        if (r == OB_NET_RESTART) snap_jump();
        if (r == 1) break;
        if (r < 0) ended(ob_net_why(net));
        else if (wp) wp();                  /* (someone's buttons are late: the window stays alive) */
        if (!net) { restart = 0; console_restart(local_tap); }
        ob_net_poll(net);
    }
    snap_mark(NULL, NULL);
    memcpy(pp, all, sizeof *pp * NETPLAY_PADS);
    uint32_t steps = ob_net_step_count(net);
    if (steps % CHECK_EVERY == 0) ob_net_check(net, steps - 1, sim_hash());   /* (again in a replay: it waits for every button) */
    return 1;
}
