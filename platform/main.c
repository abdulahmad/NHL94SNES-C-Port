/* NHL '94 in a window (SDL2): snes94 [ROM]. The ROM is the player's own dump of the US cartridge
 * (a .smc's copier header is skipped); without one on the command line, settings.ini's game.rom,
 * else the first .smc beside the program (or in the working directory) that is the cartridge. The game runs on this thread; at each frame's end the
 * window's events are handled, the picture is drawn, the frame is timed by the sound queue (by the
 * clock without a sound device) and the pads are read for the next.
 *
 * Up to five players: keys and a controller each, set in settings.ini (written beside the program
 * the first time, with every setting). The multitap is in port 2 when a player 3-5 has keys or a
 * controller at power on, as the cartridge needs it for more than two. F9 (and nothing else) opens
 * the settings menu (platform/menu.h); Escape (or F9) closes it, and the game waits while it is
 * open. Its items, each change saved to settings.ini at once:
 *   Controllers...  opens the controller setup screen (platform/padsetup.h: each button asked for in
 *                   turn, then the stick's dead zone; saved to settings.ini); closing it returns to the menu
 *   Show FPS        Off / On (window.show_fps, default Off): the game's new pictures a second, counted
 *                   over each second of real time and drawn in the picture's top right corner (a still
 *                   screen reads 0; in play at Original speed about 30, a pass of the game taking 2 vblanks)
 *   Picture         4:3 / Native (window.aspect, default 4:3): the picture's shape, a TV's, or its
 *                   pixels square at a whole scale
 *   Game Speed      Original / Smooth (game.speed, default Original): the cartridge's time, or the
 *                   game's passes 8 times as fast (platform/clock.c)
 * Menu: Up / Down choose, Enter (or Left / Right) changes; controller: d-pad or stick up / down,
 * south button (A) or Start changes, Back closes.
 *   pad 1: arrows, Z / X / A / S = B / A / Y / X, Q / W = L / R, Right Shift = Select, Enter = Start
 *   pad 2: I J K L, F / G / R / T, E / Y, Left Shift, Tab
 *   controller N is player N's: d-pad or left stick; south / east / west / north = B / A / Y / X
 * File: settings.ini, beside the program. */
#include <SDL2/SDL.h>
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

#define SCALE 3
#define RATE 32040                      /* the sound's samples a second (the sound chip's, as snes9x has it) */
#define QUEUE_FRAMES 3                  /* sound queued before the next frame goes */
#define SETTINGS "settings.ini"
#define ROM_EXT ".smc"           /* the ROM looked for beside the program */
#define PADS 5
#define SLOTS 8                         /* controllers, each in the slot it was given when plugged in */
#define NB 12
#define FPS_W 48                        /* the counter's own small picture */
#define FPS_H 10

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

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex, *setup_tex, *fps_tex;
static SDL_AudioDeviceID dev;
static uint32_t picture[SNRT_WIDTH * SNRT_HEIGHT], setup_px[SNRT_WIDTH * SNRT_HEIGHT], fps_px[FPS_W * FPS_H];
static int16_t sound[2 * 2048];         /* the frame's sound */
static int n_sound;
static uint64_t next_tick;
static char base[1024];                 /* the folder the program is in (empty: the working directory) */

static pm_settings *set;
static SDL_Scancode key[PADS][NB];
static SDL_GameController *ctrl[SLOTS];
static int player_ctrl[PADS];           /* each player's controller slot + 1 (0: none) */
static int cbutton[NB];                 /* the controllers' button for each of the pad's (the directions' unused) */
static int dead_zone;                   /* the left stick's, in % */

/* the settings menu: open, the item chosen, each controller's stick held up or down (-1, 1; 0: not) */
enum { ITEM_CONTROLLERS, ITEM_FPS, ITEM_PICTURE, ITEM_SPEED, ITEMS };
static int menu, menu_at, show_fps, stick_dir[SLOTS];

/* the picture's shape: a TV's 4:3, as the console's picture filled one (1), or its pixels square (0) */
static int tv;

/* the frame counter: the new pictures since fps_since (ticks), the last second's count shown */
static int fps_frames, fps_shown;
static uint32_t fps_since;

/* the controller setup screen: open, the controller it sets (a slot; -1: still to choose), the pad's
 * button asked for (-1: the dead zone) and since when, what it has set so far; each slot's
 * triggers past halfway */
static int setup, setup_slot, setup_at, setup_dead, setup_map[NB];
static uint32_t setup_since;
static uint8_t trigger_down[SLOTS][2];

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
            "# 1-8 as they are plugged in). F9 opens the settings menu in the game (Controllers sets a\n"
            "# controller up). The multitap is in the ports when player 3, 4 or 5 has keys or a controller\n"
            "# plugged in at power on.\n"
            "# window.show_fps: 1 shows the game's new pictures a second in the picture's top right corner\n"
            "# (0 off). window.aspect: 4:3 (the default) shows the picture as a TV did, native with its\n"
            "# pixels square. game.speed: original (the default) gives the game the cartridge's time, smooth\n"
            "# runs its passes 8 times as fast. All three are set in the game's menu, too.\n";
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
    pm_settings_default(set, "window.aspect", "4:3");
    show_fps = pm_settings_get_int(set, "window.show_fps", 0) != 0;
    const char *aspect = pm_settings_get(set, "window.aspect");
    tv = !aspect || strcmp(aspect, "native") != 0;        /* (anything but native: 4:3) */
    const char *speed = pm_settings_get(set, "game.speed");
    original_speed = !speed || strcmp(speed, "smooth") != 0; /* (anything but smooth: Original) */
    settings_write();                                     /* (a new file, or new settings: written now) */
}

/* ---- controllers ---- */

/* a controller plugged in (device index): into the first free slot, unless it is open already */
static void ctrl_added(int index)
{
    if (!SDL_IsGameController(index)) return;
    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
    for (int i = 0; i < SLOTS; i++)
        if (ctrl[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(ctrl[i])) == id) return;
    for (int i = 0; i < SLOTS; i++)
        if (!ctrl[i]) { ctrl[i] = SDL_GameControllerOpen(index); break; }
}

/* a controller pulled out (instance id): its slot empty, the others where they were */
static void ctrl_removed(SDL_JoystickID id)
{
    for (int i = 0; i < SLOTS; i++)
        if (ctrl[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(ctrl[i])) == id) {
            SDL_GameControllerClose(ctrl[i]);
            ctrl[i] = NULL;
        }
}

static int slot_of(SDL_JoystickID id)
{
    for (int i = 0; i < SLOTS; i++) if (ctrl[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(ctrl[i])) == id) return i;
    return -1;
}

static SDL_GameController *player_pad(int p) { return player_ctrl[p] ? ctrl[player_ctrl[p] - 1] : NULL; }

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
    if (setup && setup_slot >= 0 && !ctrl[setup_slot]) { setup_close(0); return; }
    if (setup && setup_at >= 0 && setup_slot >= 0 && SDL_GetTicks() - setup_since >= 5000) {
        if (++setup_at == NB) setup_at = -1;
        setup_since = SDL_GetTicks();
    }
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
    SDL_GameController *c = setup_slot >= 0 ? ctrl[setup_slot] : NULL;
    int none = 1;
    for (int i = 0; i < SLOTS; i++) if (ctrl[i]) none = 0;
    const char *cname = c ? SDL_GameControllerName(c) : NULL;
    pm_padsetup_view v = { .controller = setup_slot < 0 ? "" : cname ? cname : "a controller", .names = name_p, .values = value_p, .roles = role_p, .n = n,
                           .at = setup_slot < 0 ? -2 : setup_at < 0 ? -1 : setup_at - 4,
                           .seconds = 5 - (int)((SDL_GetTicks() - setup_since) / 1000), .dead_zone = setup_dead,
                           .stick_x = c ? SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) : 0,
                           .stick_y = c ? SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY) : 0, .none = none };
    pm_padsetup_draw(setup_px, SNRT_WIDTH, SNRT_HEIGHT, SNRT_WIDTH, &v);
    SDL_UpdateTexture(setup_tex, NULL, setup_px, SNRT_WIDTH * 4);
    SDL_RenderCopy(ren, setup_tex, NULL, dst);
}

/* ---- the settings menu ---- */

/* the item chosen, or toggled: Controllers... opens the setup screen (over the menu, which comes
 * back when it closes); the others flip at once and are saved */
static void menu_change(int item)
{
    if (item == ITEM_CONTROLLERS) { setup_open(-1); return; }
    if (item == ITEM_FPS) {
        show_fps = !show_fps;
        pm_settings_set_int(set, "window.show_fps", show_fps);
        fps_frames = 0;
        fps_since = SDL_GetTicks();
    } else if (item == ITEM_PICTURE) {
        tv = !tv;
        pm_settings_set(set, "window.aspect", tv ? "4:3" : "native");
    } else {
        original_speed = !original_speed;
        pm_settings_set(set, "game.speed", original_speed ? "original" : "smooth");
    }
    settings_write();
}

static void menu_move(int d) { menu_at = (menu_at + d + ITEMS) % ITEMS; }

static void menu_key(SDL_Scancode k)
{
    if (k == SDL_SCANCODE_ESCAPE || k == SDL_SCANCODE_F9) menu = 0;
    else if (k == SDL_SCANCODE_UP) menu_move(-1);
    else if (k == SDL_SCANCODE_DOWN) menu_move(1);
    else if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER) menu_change(menu_at);
    else if ((k == SDL_SCANCODE_LEFT || k == SDL_SCANCODE_RIGHT) && menu_at != ITEM_CONTROLLERS) menu_change(menu_at);
}

static void menu_button(int button)
{
    if (button == SDL_CONTROLLER_BUTTON_DPAD_UP) menu_move(-1);
    else if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) menu_move(1);
    else if (button == SDL_CONTROLLER_BUTTON_A || button == SDL_CONTROLLER_BUTTON_START) menu_change(menu_at);
    else if (button == SDL_CONTROLLER_BUTTON_BACK) menu = 0;
}

/* the menu over the picture, at the picture's resolution */
static void menu_draw(const SDL_Rect *dst)
{
    const char *names[ITEMS] = { "Controllers...", "Show FPS", "Picture", "Game Speed" };
    const char *values[ITEMS] = { "", show_fps ? "On" : "Off", tv ? "4:3" : "Native", original_speed ? "Original" : "Smooth" };
    pm_menu_settings_view v = { .names = names, .values = values, .n = ITEMS, .at = menu_at };
    pm_menu_settings_draw(setup_px, SNRT_WIDTH, SNRT_HEIGHT, SNRT_WIDTH, &v);
    SDL_UpdateTexture(setup_tex, NULL, setup_px, SNRT_WIDTH * 4);
    SDL_RenderCopy(ren, setup_tex, NULL, dst);
}

/* ---- the frame counter ---- */

/* each second of real time: the count of new pictures becomes the number shown */
static void fps_tick(void)
{
    uint32_t now = SDL_GetTicks(), ms = now - fps_since;
    if (ms < 1000) return;
    fps_shown = (int)((fps_frames * 1000u + ms / 2) / ms);
    fps_frames = 0;
    fps_since = now;
}

/* the number in the picture's top right corner, over the game (a small picture of its own, the
 * magenta key becoming see-through) */
static void fps_draw(const SDL_Rect *dst)
{
    char s[16];
    snprintf(s, sizeof s, "%d FPS", fps_shown);
    for (int i = 0; i < FPS_W * FPS_H; i++) fps_px[i] = 0x00FF00FFu;
    pm_menu_text(fps_px, FPS_W, FPS_H, FPS_W, FPS_W - 1 - (int)strlen(s) * PM_MENU_CELL_W, 0, s, 0xFFFFFF);
    for (int i = 0; i < FPS_W * FPS_H; i++)
        fps_px[i] = fps_px[i] == 0x00FF00FFu ? 0 : fps_px[i] | 0xFF000000u;
    SDL_UpdateTexture(fps_tex, NULL, fps_px, FPS_W * 4);
    SDL_Rect src = { 0, 0, FPS_W, FPS_H };
    SDL_Rect to = { dst->x + (SNRT_WIDTH - FPS_W) * dst->w / SNRT_WIDTH, dst->y, FPS_W * dst->w / SNRT_WIDTH, FPS_H * dst->w / SNRT_WIDTH };
    SDL_RenderCopy(ren, fps_tex, &src, &to);
}

/* ---- each frame ---- */

static void quit(void)
{
    settings_write();
    SDL_Quit();
    exit(0);
}

/* the window's events: closing, F9, controllers plugged in and out, the setup screen's presses */
static void events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) quit();
        else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (setup) setup_key(e.key.keysym.scancode);
            else if (menu) menu_key(e.key.keysym.scancode);
            else if (e.key.keysym.scancode == SDL_SCANCODE_F9) { menu = 1; menu_at = 0; }
        }
        else if (e.type == SDL_CONTROLLERBUTTONDOWN && (setup || menu)) {
            int slot = slot_of(e.cbutton.which);
            if (slot >= 0 && setup) setup_button(slot, e.cbutton.button);
            else if (slot >= 0) menu_button(e.cbutton.button);
        }
        else if (e.type == SDL_CONTROLLERAXISMOTION && e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int slot = slot_of(e.caxis.which);          /* (the stick up or down moves in the menu, once a push) */
            if (slot >= 0) {
                int v = e.caxis.value;
                int d = v < -16384 ? -1 : v > 16384 ? 1 : v > -8192 && v < 8192 ? 0 : stick_dir[slot];
                if (d && d != stick_dir[slot] && menu && !setup) menu_move(d);
                stick_dir[slot] = d;
            }
        }
        else if (e.type == SDL_CONTROLLERAXISMOTION && (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) {
            int slot = slot_of(e.caxis.which), k = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
            if (slot >= 0 && e.caxis.value > 16384 && !trigger_down[slot][k]) {   /* (pulled past halfway: once, until let go) */
                trigger_down[slot][k] = 1;
                if (setup) setup_button(slot, k ? TRIGGER_R : TRIGGER_L);
            } else if (slot >= 0 && e.caxis.value < 8192) trigger_down[slot][k] = 0;
        }
        else if (e.type == SDL_CONTROLLERDEVICEADDED) ctrl_added(e.cdevice.which);
        else if (e.type == SDL_CONTROLLERDEVICEREMOVED) ctrl_removed(e.cdevice.which);
    }
}

/* the picture as large as fits, 4:3, or its pixels square at the largest whole scale that fits, and
 * the setup screen or the menu over it when open (stretched with it) */
static void draw(void)
{
    int ww, wh;
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    int aw = tv ? 4 : SNRT_WIDTH, ah = tv ? 3 : SNRT_HEIGHT;
    int w = ww, h = ww * ah / aw;
    if (h > wh) { h = wh; w = wh * aw / ah; }
    int scale = ww / SNRT_WIDTH < wh / SNRT_HEIGHT ? ww / SNRT_WIDTH : wh / SNRT_HEIGHT;
    if (!tv && scale >= 1) { w = SNRT_WIDTH * scale; h = SNRT_HEIGHT * scale; }   /* (a window smaller than 1x: as large as fits) */
    SDL_Rect dst = { (ww - w) / 2, (wh - h) / 2, w, h };
    SDL_UpdateTexture(tex, NULL, picture, SNRT_WIDTH * 4);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, &dst);
    if (show_fps) fps_draw(&dst);
    if (setup) setup_draw(&dst);
    else if (menu) menu_draw(&dst);
    SDL_RenderPresent(ren);
}

/* the pads: each player's keys and controller (none while the setup screen is open) */
static void read_pads(void)
{
    const Uint8 *down = SDL_GetKeyboardState(NULL);
    for (int p = 0; p < PADS; p++) {
        uint16_t b = 0;
        for (int i = 0; i < NB; i++) if (key[p][i] != SDL_SCANCODE_UNKNOWN && down[key[p][i]]) b |= buttons[i].bit;
        SDL_GameController *c = player_pad(p);
        if (c) b |= ctrl_buttons(c);
        machine.io.pad[p] = setup ? 0 : b;
    }
}

void console_sample(int16_t l, int16_t r)
{
    if (n_sound < 2048) { sound[2 * n_sound] = l; sound[2 * n_sound + 1] = r; n_sound++; }
}

/* the frame's end: the picture, the window, the timing, the pads for the next */
void console_frame_done(uint32_t frame)
{
    (void)frame;
    int changed = 0;                                      /* (a picture that differs from the one before counts for the frame counter) */
    for (int i = 0; i < SNRT_WIDTH * SNRT_HEIGHT; i++) {   /* (RGB565 to ARGB) */
        unsigned c = machine.picture[i], r = c >> 11, g = c >> 5 & 63, b = c & 31;
        uint32_t argb = 0xFF000000u | (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
        changed |= picture[i] != argb;
        picture[i] = argb;
    }
    fps_frames += changed;
    do {                                                  /* (the game waits while the menu or the setup screen is open) */
        events();
        setup_tick();
        fps_tick();
        draw();
        if (setup || menu) SDL_Delay(15);
    } while (setup || menu);
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

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    char *where = SDL_GetBasePath();
    if (where) { snprintf(base, sizeof base, "%s", where); SDL_free(where); }
    settings_load();
    const char *rom = argc > 1 ? argv[1] : pm_settings_get(set, "game.rom");
    static char found[1300];
    const char *why = NULL;
    if (rom && *rom) why = not_nhl94(rom);
    else if (!(rom = find_rom(found, sizeof found))) rom = "none", why = "no " ROM_EXT " file beside the program is NHL '94";
    if (why) {                                            /* (a box: a player who opened the program sees it) */
        char msg[1600];
        snprintf(msg, sizeof msg, "No game ROM: %s: %s.\n\nDrag your NHL '94 ROM onto the program, or put it beside the program (any "
                 "name ending " ROM_EXT "), or give its path as game.rom in " SETTINGS " (beside the program).", rom, why);
        fprintf(stderr, "%s\n", msg);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "NHL '94", msg, NULL);
        return 1;
    }
    win = SDL_CreateWindow("NHL '94", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (tv ? SNRT_HEIGHT * 4 / 3 : SNRT_WIDTH) * SCALE, SNRT_HEIGHT * SCALE, SDL_WINDOW_RESIZABLE);
    ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (!ren && win) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SNRT_WIDTH, SNRT_HEIGHT) : NULL;
    setup_tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SNRT_WIDTH, SNRT_HEIGHT) : NULL;
    fps_tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FPS_W, FPS_H) : NULL;
    if (!tex || !setup_tex || !fps_tex) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_SetTextureBlendMode(setup_tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(fps_tex, SDL_BLENDMODE_BLEND);
    fps_since = SDL_GetTicks();
    SDL_AudioSpec want = { .freq = RATE, .format = AUDIO_S16SYS, .channels = 2, .samples = 512 }, got;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (!dev) fprintf(stderr, "no sound (%s): the frames keep time by the clock\n", SDL_GetError());
    for (int i = 0; i < SDL_NumJoysticks(); i++) ctrl_added(i);
    clock_open();
    machine.io.multitap = (uint8_t)multitap();
    console_run();
    return 0;
}
