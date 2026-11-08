/* NHL '94 in a window (SDL2): snes94 [ROM]. The ROM is the player's own dump of the US cartridge
 * (a .smc's copier header is skipped); without one on the command line, settings.ini's game.rom,
 * else the first .smc beside the program (or in the working directory) that is the cartridge. The game runs on this thread; at each frame's end the
 * window's events are handled, the picture is drawn, the frame is timed by the sound queue (by the
 * clock without a sound device) and the pads are read for the next.
 *
 * Up to five players: keys and a controller each, set in settings.ini (written beside the program
 * the first time, with every setting). The multitap is in port 2 when a player 3-5 has keys or a
 * controller at power on, as the cartridge needs it for more than two. ` and F11 (and a controller's
 * Back) open the Overbar overlay; F12 toggles stats. Escape does not open it. The game waits while
 * the overlay or the controller setup screen is open. Video is the shared Scale Mode tree; the
 * picture has the pixel shape of a 4:3 screen of 240 lines (PAR 5:4), the 224 real lines drawn. There is
 * no Picture Native. Game Speed is an overlay addon. Controllers... is on Input (and the menu bar's).
 * Online play (the overlay's Online page, platform/netplay.c): lockstep, the frames its steps; the
 * game does not wait for the overlay or the setup screen in a session, whose pads read 0 meanwhile.
 *   pad 1: arrows, Z / X / A / S = B / A / Y / X, Q / W = L / R, Right Shift = Select, Enter = Start
 *   pad 2: I J K L, F / G / R / T, E / Y, Left Shift, Tab
 *   controller N is player N's: d-pad or left stick; south / east / west / north = B / A / Y / X
 * File: settings.ini, beside the program. */
#include <SDL2/SDL.h>
#include "overbar/plat.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rom.h"
#include "console.h"
#include "menu.h"
#include "padsetup.h"
#include "settings.h"
#include "overbar_client.h"
#include "netplay.h"

#define SCALE 3
#define RATE 32040                      /* the sound's samples a second (the sound chip's, as snes9x has it) */
#define QUEUE_FRAMES 3                  /* sound queued before the next frame goes */
#define SETTINGS "settings.ini"
#define ROM_EXT ".smc"           /* the ROM looked for beside the program */
#define PADS 5
#define SLOTS 8                         /* controllers, each in the slot it was given when plugged in */
#define NB 12

/* the pad's buttons: name in settings.ini, bit, default keys for pads 1 and 2, default controller
 * button (south B, east A, west Y, north X), and what each does with the puck and without (the
 * setup screen shows it; game/play/input.c's doinput). The first four are the directions. */
static const struct { const char *name; uint16_t bit; const char *key1, *key2, *ctrl, *role; } buttons[NB] = {
    { "up", 0x0800, "Up", "I", "", NULL }, { "down", 0x0400, "Down", "K", "", NULL },
    { "left", 0x0200, "Left", "J", "", NULL }, { "right", 0x0100, "Right", "L", "", NULL },
    { "b", 0x8000, "Z", "F", "a", "pass, switch" }, { "a", 0x0080, "X", "G", "b", "shoot, check/burst" },
    { "y", 0x4000, "A", "R", "x", "dump in, hook/hold" }, { "x", 0x0040, "S", "T", "y", "take goalie" },
    { "l", 0x0020, "Q", "E", "leftshoulder", "take left D" }, { "r", 0x0010, "W", "Y", "rightshoulder", "take right D" },
    { "select", 0x2000, "Right Shift", "Left Shift", "back", NULL }, { "start", 0x1000, "Return", "Tab", "start", "pause" },
};
enum { START = 11 };

static SDL_Texture *setup_tex;
static SDL_AudioDeviceID dev;
static uint32_t picture[SNRT_WIDTH * SNRT_HEIGHT], setup_px[SNRT_WIDTH * SNRT_HEIGHT];
static int16_t sound[2 * 2048];         /* the frame's sound */
static int n_sound;
static uint64_t next_tick;
static char base[1024];                 /* the folder the program is in (empty: the working directory) */

static pm_settings *set;
static SDL_Scancode key[PADS][NB];
static int player_ctrl[PADS];           /* each player's controller slot + 1 (0: none) */
static int cbutton[NB];                 /* the controllers' button for each of the pad's (the directions' unused) */
static int dead_zone;                   /* the left stick's, in % */

/* the settings overlay: Overbar; each controller's stick held up or down (-1, 1; 0: not) */
static int show_fps;


/* the controller setup screen: open, the controller it sets (a slot; -1: still to choose), the pad's
 * button asked for (-1: the dead zone) and since when, what it has set so far; each slot's
 * triggers past halfway */
static int setup, setup_slot, setup_at, setup_dead, setup_map[NB];
static uint32_t setup_since;

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* a file beside the program */
static const char *beside(const char *name)
{
    static char path[1100];
    snprintf(path, sizeof path, "%s%s", base, name);
    return path;
}

/* ---- settings.ini ---- */

static void settings_write(void)
{
    if (!pm_settings_dirty(set)) return;
    size_t n = pm_settings_format(set, NULL, 0);
    char *text = malloc(n);
    FILE *f = text ? fopen(beside(SETTINGS), "wb") : NULL;
    if (f) {
        pm_settings_format(set, text, n);
        if (fwrite(text, 1, n - 1, f) == n - 1) pm_settings_clean(set);
        fclose(f);
    }
    free(text);
}

static SDL_Scancode bound(const char *name)
{
    SDL_Scancode k = name && *name ? SDL_GetScancodeFromName(name) : SDL_SCANCODE_UNKNOWN;
    if (name && *name && k == SDL_SCANCODE_UNKNOWN) fprintf(stderr, "settings: no key called \"%s\"\n", name);
    return k;
}

/* a controller's input as settings.ini names it: one of SDL's buttons, or a trigger, which counts as
 * pressed past halfway (many controllers' L and R) */
enum { TRIGGER_L = 100, TRIGGER_R };
static int cinput_from(const char *v)
{
    if (!strcmp(v, "lefttrigger")) return TRIGGER_L;
    if (!strcmp(v, "righttrigger")) return TRIGGER_R;
    return SDL_GameControllerGetButtonFromString(v);
}
static const char *cinput_name(int v)
{
    if (v == TRIGGER_L) return "lefttrigger";
    if (v == TRIGGER_R) return "righttrigger";
    const char *n = v != SDL_CONTROLLER_BUTTON_INVALID ? SDL_GameControllerGetStringForButton((SDL_GameControllerButton)v) : NULL;
    return n ? n : "";
}
static int cinput_held(SDL_GameController *c, int v)
{
    if (v == TRIGGER_L || v == TRIGGER_R)
        return SDL_GameControllerGetAxis(c, v == TRIGGER_L ? SDL_CONTROLLER_AXIS_TRIGGERLEFT : SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16384;
    return v != SDL_CONTROLLER_BUTTON_INVALID && SDL_GameControllerGetButton(c, (SDL_GameControllerButton)v);
}

/* the file read (or begun), every setting given its default, the keys looked up */
static void settings_load(void)
{
    set = pm_settings_new();
    FILE *f = fopen(beside(SETTINGS), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *text = n > 0 ? malloc((size_t)n) : NULL;
        if (text && fread(text, 1, (size_t)n, f) == (size_t)n) pm_settings_parse(set, text, (size_t)n);
        free(text);
        fclose(f);
    } else {
        static const char head[] =
            "# NHL '94's settings: edit them while the game is closed. Keys are SDL's key names, empty for\n"
            "# none: A, 1, F1, Up, Return, Space, Tab, Backspace, Left Shift, Right Shift, Keypad 8, or the\n"
            "# key's own sign (, . - =). Controller buttons are SDL's names: a b x y back start leftshoulder\n"
            "# rightshoulder leftstick rightstick guide, and lefttrigger righttrigger (pressed past halfway).\n"
            "# playerN.controller: the controller slot player N plays with (0: none; controllers take slots\n"
            "# 1-8 as they are plugged in). ` or F11 opens the Overbar overlay (Controllers sets a\n"
            "# controller up). F12 toggles stats. The multitap is in the ports when player 3, 4 or 5 has keys or a controller\n"
            "# plugged in at power on.\n"
            "# window.show_fps: 1 shows the game's new pictures a second in the picture's top right corner\n"
            "# (0 off). The picture has the pixel shape of a 4:3 screen of 240 lines, only the 224 drawn (no Native). game.speed: original (the default) gives the game the cartridge's time, smooth\n"
            "# runs its passes 8 times as fast. Overlay Video live-previews Scale Mode.\n";
        pm_settings_parse(set, head, sizeof head - 1);
    }
    pm_settings_default(set, "game.rom", "");
    for (int p = 0; p < PADS; p++) {
        char k[48], slot[4];
        for (int b = 0; b < NB; b++) {
            snprintf(k, sizeof k, "pad%d.%s", p + 1, buttons[b].name);
            pm_settings_default(set, k, p == 0 ? buttons[b].key1 : p == 1 ? buttons[b].key2 : "");
            key[p][b] = bound(pm_settings_get(set, k));
        }
        snprintf(k, sizeof k, "player%d.controller", p + 1);
        snprintf(slot, sizeof slot, "%d", p + 1);
        pm_settings_default(set, k, slot);
        player_ctrl[p] = clamp(pm_settings_get_int(set, k, p + 1), 0, SLOTS);
    }
    for (int b = 0; b < NB; b++) {
        cbutton[b] = SDL_CONTROLLER_BUTTON_INVALID;
        if (b < 4) continue;                              /* (the directions: the d-pad and the stick) */
        char k[48];
        snprintf(k, sizeof k, "controller.%s", buttons[b].name);
        pm_settings_default(set, k, buttons[b].ctrl);
        const char *v = pm_settings_get(set, k);
        cbutton[b] = v && *v ? cinput_from(v) : SDL_CONTROLLER_BUTTON_INVALID;
        if (v && *v && cbutton[b] == SDL_CONTROLLER_BUTTON_INVALID) fprintf(stderr, "settings: no controller button called \"%s\"\n", v);
    }
    pm_settings_default(set, "controller.dead_zone", "50");
    dead_zone = clamp(pm_settings_get_int(set, "controller.dead_zone", 50), 5, 95);
    pm_settings_default(set, "window.show_fps", "0");
    pm_settings_default(set, "game.speed", "original");
    show_fps = pm_settings_get_int(set, "window.show_fps", 0) != 0;
    const char *speed = pm_settings_get(set, "game.speed");
    original_speed = !speed || strcmp(speed, "smooth") != 0; /* (anything but smooth: Original) */
    settings_write();                                     /* (a new file, or new settings: written now) */
}

/* ---- controllers ---- */

static SDL_GameController *player_pad(int p) { return player_ctrl[p] ? ob_plat_controller(player_ctrl[p] - 1) : NULL; }

/* a controller's buttons held, as the pad's */
static uint16_t ctrl_buttons(SDL_GameController *c)
{
    static const SDL_GameControllerButton dpad[4] = { SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                                                      SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT };
    uint16_t b = 0;
    for (int i = 0; i < 4; i++) if (SDL_GameControllerGetButton(c, dpad[i])) b |= buttons[i].bit;
    for (int i = 4; i < NB; i++) if (cinput_held(c, cbutton[i])) b |= buttons[i].bit;
    int x = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), y = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    int dead = 32767 * dead_zone / 100;
    if (y < -dead) b |= buttons[0].bit;
    if (y > dead) b |= buttons[1].bit;
    if (x < -dead) b |= buttons[2].bit;
    if (x > dead) b |= buttons[3].bit;
    return b;
}

/* the multitap: a player 3-5 has a controller plugged in, or keys */
static int multitap(void)
{
    for (int p = 2; p < PADS; p++) {
        if (player_pad(p)) return 1;
        for (int b = 0; b < NB; b++) if (key[p][b] != SDL_SCANCODE_UNKNOWN) return 1;
    }
    return 0;
}

/* ---- the controller setup screen ---- */

/* opened for the controller in slot (-1: the player picks it by pressing a button on it) */
static void setup_open(int slot)
{
    setup = 1;
    setup_slot = slot;
    setup_at = 4;                                         /* (the directions are the d-pad's and the stick's) */
    setup_since = SDL_GetTicks();
    memcpy(setup_map, cbutton, sizeof setup_map);
    setup_dead = dead_zone;
}

static void setup_close(int save)
{
    if (save) {
        for (int i = 4; i < NB; i++) {
            char k[48];
            snprintf(k, sizeof k, "controller.%s", buttons[i].name);
            pm_settings_set(set, k, cinput_name(setup_map[i]));
            cbutton[i] = setup_map[i];
        }
        dead_zone = setup_dead;
        pm_settings_set_int(set, "controller.dead_zone", dead_zone);
        settings_write();
    }
    setup = 0;
}

/* a controller's button pressed while the screen is open */
static void setup_button(int slot, int button)
{
    if (setup_slot < 0) { setup_open(slot); return; }     /* (the controller to set up) */
    if (slot != setup_slot) return;
    int dpad = button >= SDL_CONTROLLER_BUTTON_DPAD_UP && button <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    if (setup_at >= 0) {
        if (dpad) return;                                 /* (the directions are not set here) */
        pm_padsetup_assign(setup_map + 4, NB - 4, setup_at - 4, button);
        if (++setup_at == NB) setup_at = -1;
        setup_since = SDL_GetTicks();
        return;
    }
    if (button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) setup_dead = clamp(setup_dead - 5, 5, 95);
    else if (button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) setup_dead = clamp(setup_dead + 5, 5, 95);
    else if (button == setup_map[START]) setup_close(1);
}

/* the keyboard while the screen is open: Escape (or F9) closes it unsaved; on the dead zone Left /
 * Right change it, Enter saves */
static void setup_key(SDL_Scancode k)
{
    if (k == SDL_SCANCODE_ESCAPE || k == SDL_SCANCODE_F9) setup_close(0);
    else if (setup_at == -1 && k == SDL_SCANCODE_LEFT) setup_dead = clamp(setup_dead - 5, 5, 95);
    else if (setup_at == -1 && k == SDL_SCANCODE_RIGHT) setup_dead = clamp(setup_dead + 5, 5, 95);
    else if (setup_at == -1 && k == SDL_SCANCODE_RETURN) setup_close(1);
}

/* a button not pressed in 5 seconds: kept as it is; the controller pulled out: closed unsaved */
static void setup_tick(void)
{
    if (setup && setup_slot >= 0 && !ob_plat_controller(setup_slot)) { setup_close(0); return; }
    if (setup && setup_at >= 0 && setup_slot >= 0 && SDL_GetTicks() - setup_since >= 5000) {
        if (++setup_at == NB) setup_at = -1;
        setup_since = SDL_GetTicks();
    }
}

/* setup_px over the picture */
static void setup_show(const SDL_Rect *dst)
{
    if (!setup_tex) {
        setup_tex = SDL_CreateTexture(ob_plat_renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SNRT_WIDTH, SNRT_HEIGHT);
        if (setup_tex) SDL_SetTextureBlendMode(setup_tex, SDL_BLENDMODE_BLEND);
    }
    if (!setup_tex) return;
    SDL_UpdateTexture(setup_tex, NULL, setup_px, SNRT_WIDTH * 4);
    SDL_RenderCopy(ob_plat_renderer(), setup_tex, NULL, dst);
}

/* the screen over the picture, at the picture's resolution */
static void setup_draw(const SDL_Rect *dst)
{
    static char names[NB][16];
    const char *name_p[NB], *value_p[NB], *role_p[NB];
    int n = NB - 4;
    for (int i = 0; i < n; i++) {
        snprintf(names[i], sizeof names[i], "%s", buttons[i + 4].name);
        for (char *q = names[i]; *q; q++) *q = (char)toupper((unsigned char)*q);
        name_p[i] = names[i];
        value_p[i] = cinput_name(setup_map[i + 4]);
        role_p[i] = buttons[i + 4].role ? buttons[i + 4].role : "";
    }
    SDL_GameController *c = setup_slot >= 0 ? ob_plat_controller(setup_slot) : NULL;
    int none = 1;
    for (int i = 0; i < SLOTS; i++) if (ob_plat_controller(i)) none = 0;
    const char *cname = c ? SDL_GameControllerName(c) : NULL;
    pm_padsetup_view v = { .controller = setup_slot < 0 ? "" : cname ? cname : "a controller", .names = name_p, .values = value_p, .roles = role_p, .n = n,
                           .at = setup_slot < 0 ? -2 : setup_at < 0 ? -1 : setup_at - 4,
                           .seconds = 5 - (int)((SDL_GetTicks() - setup_since) / 1000), .dead_zone = setup_dead,
                           .stick_x = c ? SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) : 0,
                           .stick_y = c ? SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY) : 0, .none = none };
    pm_padsetup_draw(setup_px, SNRT_WIDTH, SNRT_HEIGHT, SNRT_WIDTH, &v);
    setup_show(dst);
}

/* ---- Overbar overlay ---- */

static void quit(void);

static void overlay_sync_settings(void)
{
    show_fps = overbar_client_stats();
    pm_settings_set_int(set, "window.show_fps", show_fps);
    if (!netplay_playing()) pm_settings_set(set, "game.speed", original_speed ? "original" : "smooth");   /* (in a session: the host's) */
    settings_write();
}

/* typed text for a field of the Online page (Name, Address, Port): SDL's text input on while one is edited */
/* ---- online play: the Online page's hooks (platform/netplay.c) ---- */

static int online_start(const ob_online *o, char *msg, int msg_size)
{
    int r = netplay_start(o, multitap(), msg, msg_size);
    return r;
}

static void online_leave(void) { netplay_leave(); }
static int online_status(char *line, int line_size) { return netplay_status(line, line_size); }

static void online_setup(void)
{
    ob_online_ops ops;
    memset(&ops, 0, sizeof ops);
    ops.max_pads = PADS;
    ops.max_delay = 10;
    ops.roles = 3;
    ops.start = online_start;
    ops.leave = online_leave;
    ops.status = online_status;
    ops.lan_address = ob_net_lan_address;
    ob_online_set_ops(overbar_client_host(), &ops);
}

/* the menu bar's Input > Controllers: the port's own setup screen */
static void shell_controllers(void) { setup_open(-1); }


/* ---- each frame ---- */

/* the player closed the game: the layer closes the window and leaves after this */
static void quit(void)
{
    netplay_leave();                                      /* (goodbye to the others, so they end at once) */
    settings_write();
}

/* online play's line (connecting, or why a session ended) along the picture's foot */
static void banner_draw(const SDL_Rect *dst)
{
    const char *line = netplay_banner(SDL_GetTicks());
    if (!line || setup) return;
    for (int i = 0; i < SNRT_WIDTH * SNRT_HEIGHT; i++) setup_px[i] = 0;
    for (int y = SNRT_HEIGHT - PM_MENU_CELL_H - 4; y < SNRT_HEIGHT; y++)
        for (int x = 0; x < SNRT_WIDTH; x++) setup_px[y * SNRT_WIDTH + x] = 0xA0000000u;
    char cut[SNRT_WIDTH / PM_MENU_CELL_W];
    snprintf(cut, sizeof cut, "%s", line);
    pm_menu_text(setup_px, SNRT_WIDTH, SNRT_HEIGHT, SNRT_WIDTH, 4, SNRT_HEIGHT - PM_MENU_CELL_H - 2, cut, 0xFFD84A);
    setup_show(dst);
}

/* while a session's step waits for someone's buttons: the window stays alive (the overlay too) */
static void net_wait(void)
{
    ob_plat_pump();
    setup_tick();
    ob_plat_present();
    SDL_Delay(1);
}

/* the pads: each player's keys and controller (none while the setup screen or the overlay is
 * open); in a session these are this machine's players' (its first ones), and all five come back
 * from the session */
static void read_pads(void)
{
    const Uint8 *down = SDL_GetKeyboardState(NULL);
    uint16_t local[PADS], all[NETPLAY_PADS];
    for (int p = 0; p < PADS; p++) {
        uint16_t b = 0;
        for (int i = 0; i < NB; i++) if (key[p][i] != SDL_SCANCODE_UNKNOWN && down[key[p][i]]) b |= buttons[i].bit;
        SDL_GameController *c = player_pad(p);
        if (c) b |= ctrl_buttons(c);
        local[p] = (setup || overbar_client_open()) ? 0 : b;
    }
    const uint16_t *pad = netplay_pads(local, all, net_wait) ? all : local;
    for (int p = 0; p < PADS; p++) machine.io.pad[p] = pad[p];
}

void console_sample(int16_t l, int16_t r)
{
    if (n_sound < 2048) { sound[2 * n_sound] = l; sound[2 * n_sound + 1] = r; n_sound++; }
}

/* the frame's end: the picture, the window, the timing, the pads for the next */
void console_frame_done(uint32_t frame)
{
    (void)frame;
    if (netplay_replaying()) {                            /* (a frame played again after a rollback: no picture, no sound, no waiting) */
        n_sound = 0;
        read_pads();
        return;
    }
    int changed = 0;                                      /* (a picture that differs from the one before counts for the frame counter) */
    for (int i = 0; i < SNRT_WIDTH * SNRT_HEIGHT; i++) {   /* (RGB565 to ARGB) */
        unsigned c = machine.picture[i], r = c >> 11, g = c >> 5 & 63, b = c & 31;
        uint32_t argb = 0xFF000000u | (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
        changed |= picture[i] != argb;
        picture[i] = argb;
    }
    ob_plat_frame_done(changed);
    int hold;
    do {                                                  /* (the game waits while the overlay or the setup screen is open, but not in a session) */
        ob_plat_pump();
        setup_tick();
        netplay_frame(SDL_GetTicks());                    /* (a session's start or end: the console from power on, not returning) */
        ob_plat_present();
        hold = (setup || overbar_client_open()) && !netplay_on();
        if (hold) SDL_Delay(15);
    } while (hold);
    if (dev) {
        SDL_QueueAudio(dev, sound, (Uint32)n_sound * 4);
        Uint32 enough = QUEUE_FRAMES * RATE / 60 * 4;
        while (SDL_GetQueuedAudioSize(dev) > enough) SDL_Delay(1);
        SDL_PauseAudioDevice(dev, 0);
    } else {                                              /* no sound: the console's rate, by the clock */
        uint64_t freq = SDL_GetPerformanceFrequency(), now = SDL_GetPerformanceCounter();
        uint64_t period = freq * SNRT_LINES * SNRT_LINE_CYCLES / 21477272;
        if (!next_tick || now > next_tick + 4 * period) next_tick = now;
        next_tick += period;
        while (SDL_GetPerformanceCounter() < next_tick) SDL_Delay(1);
    }
    n_sound = 0;
    read_pads();
}

/* the retail cartridge: a reason the file is not it, or NULL */
static const char *not_nhl94(const char *path)
{
    if (!rom_load(path)) return "cannot read it";
    if (rom_size < 0x8000 || memcmp(rom_cart + 0x7FC0, "NHL '94", 7)) return "not an NHL '94 SNES ROM";
    if (rom_cart[0x7FD5] != 0x30) return "not the original NHL '94 (an SA-1 ROM)";
    return NULL;
}

/* name ends in ext (lower case), in any case */
static int has_ext(const char *name, const char *ext)
{
    size_t n = strlen(name), k = strlen(ext);
    if (n <= k) return 0;
    for (size_t i = 0; i < k; i++) if (tolower((unsigned char)name[n - k + i]) != ext[i]) return 0;
    return 1;
}

/* with no ROM named: the first ROM_EXT file in the program's folder, then in the working
 * directory, that is the cartridge (its path in found, the cartridge loaded); NULL when none is */
static const char *find_rom(char *found, size_t size)
{
    const char *dirs[] = { base, "" };    /* (base ends in a separator) */
    for (int i = 0; i < 2; i++) {
        DIR *d = opendir(*dirs[i] ? dirs[i] : ".");
        if (!d) continue;
        for (struct dirent *e; (e = readdir(d)); ) {
            if (!has_ext(e->d_name, ROM_EXT)) continue;
            snprintf(found, size, "%s%s", dirs[i], e->d_name);
            if (!not_nhl94(found)) { closedir(d); return found; }
        }
        closedir(d);
    }
    return NULL;
}

/* ---- the platform layer's hooks (Overbar plat.h) ---- */

static const uint32_t *hook_picture(void) { return picture; }
static int hook_action(int id) { if (id == OB_ID_CONTROLLERS) setup_open(-1); return 1; }
static int hook_captures(void) { return setup; }
static void hook_raw_key(int scancode) { setup_key((SDL_Scancode)scancode); }
static void hook_raw_button(int slot, int button) { setup_button(slot, button == -1 ? TRIGGER_L : button == -2 ? TRIGGER_R : button); }
static void hook_draw_over(const SDL_Rect *dst)
{
    banner_draw(dst);
    if (setup) setup_draw(dst);
}

int main(int argc, char *argv[])
{
    ob_profile profile = ob_profile_from_env();
    if (!ob_plat_init(&profile)) return 1;
    char *where = SDL_GetBasePath();
    if (where) { snprintf(base, sizeof base, "%s", where); SDL_free(where); }
    settings_load();
    const char *rom = argc > 1 ? argv[1] : pm_settings_get(set, "game.rom");
    static char found[1300];
    const char *why = NULL;
    if (rom && *rom) why = not_nhl94(rom);
    else if (!(rom = find_rom(found, sizeof found))) rom = "none", why = "no " ROM_EXT " file beside the program is NHL '94";
    if (why) {                                            /* (a box: a player who opened the program sees it; KMSDRM has no box) */
        char msg[1600];
        snprintf(msg, sizeof msg, "No game ROM: %s: %s.\n\nDrag your NHL '94 ROM onto the program, or put it beside the program (any "
                 "name ending " ROM_EXT "), or give its path as game.rom in " SETTINGS " (beside the program).", rom, why);
        fprintf(stderr, "%s\n", msg);
        if (profile != OB_PROFILE_HANDHELD)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "NHL '94", msg, NULL);
        return 1;
    }
    overbar_client_init(profile);
    overbar_client_set_stats(show_fps);
    online_setup();
    {
        ob_shell shell;
        memset(&shell, 0, sizeof shell);
        shell.controllers = shell_controllers;
        ob_shell_set(&shell);
    }
    {
        static const ob_plat_hooks hooks = {
            .title = "NHL '94", .win_w = 320 * SCALE, .win_h = 224 * SCALE, .cls = OB_CLASS_LORES,
            .src_w = SNRT_WIDTH, .src_h = SNRT_HEIGHT, .picture = hook_picture,
            .changed = overlay_sync_settings, .action = hook_action, .captures = hook_captures,
            .raw_key = hook_raw_key, .raw_button = hook_raw_button, .draw_over = hook_draw_over, .quit = quit,
        };
        if (!ob_plat_open(&hooks, overbar_client_host())) return 1;
    }
    SDL_AudioSpec want = { .freq = RATE, .format = AUDIO_S16SYS, .channels = 2, .samples = 512 }, got;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (!dev) fprintf(stderr, "no sound (%s): the frames keep time by the clock\n", SDL_GetError());
    clock_open();
    machine.io.multitap = (uint8_t)multitap();
    console_run();
    return 0;
}
