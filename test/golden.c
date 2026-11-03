/* The game headless, for checking that it plays bit for bit as it did: no window, the pads from a
 * script, and a line a frame of hashes (work RAM; VRAM, CGRAM and OAM; the sound CPU's RAM; the
 * picture; the frame's sound), so two runs can be compared whole (test/check.sh).
 *   golden ROM --frames N --golden OUT [--smooth] [--period30] [--script FILE]
 * --smooth (Game Speed Smooth) runs the game loop's passes 8 times as fast (platform/clock.c); --period30 makes the
 * Game Setup's 5-minute periods 30 seconds (the ROM's period table's first entry set to 30 once
 * loaded, the cartridge itself having no shorter game), so a whole game fits a short run. A script is "FRAME:BUTTON+BUTTON,..." (# lines are comments): pad 1's buttons held from that frame
 * on, "2." to "5." before a button for pads 2-5 (any of pads 3-5 puts the multitap in port 2). */
#define _POSIX_C_SOURCE 200809L   /* strtok_r (glibc declares it only for POSIX; MinGW always) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "rom.h"
#include "console.h"

static FILE *golden;
static uint32_t frames;
static uint64_t sound;

typedef struct { uint32_t frame; uint16_t pad[5]; } press;
static press *script;
static int n_script, script_at, script_tap;

static uint64_t fnv(const void *p, size_t n, uint64_t h)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001B3ull;
    return h;
}
#define FNV0 0xCBF29CE484222325ull

static uint16_t button(const char *b)
{
    static const char *names[] = { "R", "L", "X", "A", "RIGHT", "LEFT", "DOWN", "UP", "START", "SELECT", "Y", "B" };
    for (int i = 0; i < 12; i++) if (!strcmp(b, names[i])) return (uint16_t)(1 << (i + 4));
    fprintf(stderr, "script: no button %s\n", b);
    exit(2);
}

static int by_frame(const void *a, const void *b)
{
    uint32_t x = ((const press *)a)->frame, y = ((const press *)b)->frame;
    return x < y ? -1 : x > y;
}

static void load_script(const char *path)
{
    char line[65536];
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        for (char *item = strtok(line, ",\r\n"); item; item = strtok(NULL, ",\r\n")) {
            char *colon = strchr(item, ':');
            if (!colon) continue;
            script = realloc(script, sizeof *script * (size_t)(n_script + 1));
            press *p = &script[n_script++];
            p->frame = (uint32_t)atoi(item);
            memset(p->pad, 0, sizeof p->pad);
            char *save, *b;
            for (b = strtok_r(colon + 1, "+", &save); b; b = strtok_r(NULL, "+", &save))
                if (b[0] >= '2' && b[0] <= '5' && b[1] == '.') {
                    p->pad[b[0] - '1'] |= button(b + 2);
                    if (b[0] >= '3') script_tap = 1;
                } else p->pad[0] |= button(b);
        }
    }
    fclose(f);
    qsort(script, (size_t)n_script, sizeof *script, by_frame);   /* (qsort is not stable: one entry a frame in the scripts) */
}

static void pads_for(uint32_t f)
{
    while (script_at < n_script && script[script_at].frame <= f) {
        memcpy(machine.io.pad, script[script_at].pad, sizeof machine.io.pad);
        script_at++;
    }
}

void console_sample(int16_t l, int16_t r)
{
    sound = (sound ^ (uint16_t)l ^ (uint32_t)(uint16_t)r << 16) * 0x100000001B3ull;
}

void console_frame_done(uint32_t frame)
{
    uint64_t video = fnv(machine.ppu.vram, sizeof machine.ppu.vram, FNV0);
    video = fnv(machine.ppu.cgram, sizeof machine.ppu.cgram, video);
    video = fnv(machine.ppu.oam, sizeof machine.ppu.oam, video);
    fprintf(golden, "%u %016llx %016llx %016llx %016llx %016llx\n", frame - 1,
            (unsigned long long)fnv(WRAM_BYTES, 0x20000, FNV0), (unsigned long long)video,
            (unsigned long long)fnv(dsp.ram, sizeof dsp.ram, FNV0),
            (unsigned long long)fnv(machine.picture, sizeof machine.picture, FNV0), (unsigned long long)sound);
    sound = 0;
    if (frame >= frames) { fclose(golden); exit(0); }
    pads_for(frame);
}

int main(int argc, char **argv)
{
    const char *rom = NULL, *out = NULL;
    int period30 = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--golden") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) load_script(argv[++i]);
        else if (!strcmp(argv[i], "--smooth")) original_speed = 0;
        else if (!strcmp(argv[i], "--period30")) period30 = 1;
        else rom = argv[i];
    }
    if (!rom || !out || !frames) { fprintf(stderr, "usage: golden ROM --frames N --golden OUT [--smooth] [--period30] [--script FILE]\n"); return 2; }
    if (!rom_load(rom)) { fprintf(stderr, "cannot load %s\n", rom); return 2; }
    if (period30) {                                       /* (GetPeriodTime_timetab's 300 seconds: 30) */
        uint8_t *cart = (uint8_t *)rom_cart;
        cart[rom_offset(SYM_GetPeriodTime_timetab)] = 30;
        cart[rom_offset(SYM_GetPeriodTime_timetab + 1)] = 0;
    }
    if (!(golden = fopen(out, "wb"))) { fprintf(stderr, "cannot write %s\n", out); return 2; }
    clock_open();
    machine.io.multitap = (uint8_t)script_tap;
    pads_for(0);
    console_run();
}
