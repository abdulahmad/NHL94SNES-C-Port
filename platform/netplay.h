/* Online play: Overbar's netplay core (overbar/net.h) under the game, with rollback (snapstate.h). No SDL here: platform/main.c
 * runs it in the window, and a headless test can run it the same way.
 *
 * One step is one frame: at each frame's end every machine gives its own players' pads
 * and gets all five, which the next frame plays with. The session starts on every machine at the
 * same point, the console powered on again (console_restart) once everyone is in, with the host's
 * Game Speed and multitap. Every 60 steps the work RAM and the sound CPU's RAM are compared.
 * The others' pads are guessed (held) so a frame never waits for them; a wrong guess loads the
 * state from before it (snapstate.h) and the frames since are played again at once, quietly
 * (netplay_replaying). A state is taken at frames that end in the game's wait for an interrupt;
 * where the game has none (a slow frame), the session guesses only as far as its last state. */
#ifndef NETPLAY_H
#define NETPLAY_H
#include <stdint.h>
#include "overbar/overbar.h"

#define NETPLAY_PADS 5

/* the Online page's Host / Connect: 0 on the way (connecting), else -1 with why in msg. The local
 * game goes on while connecting. local_multitap: this machine's own (for after the session) */
int netplay_start(const ob_online *o, int local_multitap, char *msg, int msg_size);
/* the Online page's Leave: goodbye to the others, then (at the frame's end) the local game again
 * from power on */
void netplay_leave(void);
/* 1 while a session goes on (connecting or playing), with what it does in line; else 0, line why
 * the last one ended ("" none) */
int netplay_status(char *line, int line_size);
int netplay_on(void);           /* a session goes on (connecting or playing): nothing may pause the game */
int netplay_playing(void);      /* the session has started (its steps are the frames) */
/* a short line for over the picture while connecting, or for a few seconds after a session ended
 * (NULL none); now_ms: any clock in milliseconds */
const char *netplay_banner(uint32_t now_ms);

/* each frame's end, before the pads: pumps the socket; at the session's start or end, the console
 * powered on again (does not return then) */
void netplay_frame(uint32_t now_ms);
/* the pads for the next frame while playing: local holds this machine's players' (its first
 * `here`), all five come back in pad. wait() runs while someone's are late (events, the picture; it
 * may call netplay_leave). 1: pad holds them; 0: not playing (pad untouched); after the session
 * ended this does not return (the local game from power on) */
int netplay_pads(const uint16_t *local, uint16_t pad[NETPLAY_PADS], void (*wait)(void));
uint32_t netplay_steps(void);   /* the session's steps run so far (0: none) */
/* the frame just run was played again after a rollback (a guess of someone's buttons was wrong):
 * no sound out, no picture, no waiting for the clock */
int netplay_replaying(void);
/* (tests) a bad network on this machine's sends: delay, jitter, loss */
void netplay_simulate(int delay_ms, int jitter_ms, int loss_pct);

#endif
