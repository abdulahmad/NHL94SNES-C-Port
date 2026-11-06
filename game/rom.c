#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rom.h"

const uint8_t *rom_cart;
uint32_t rom_size;

/* The cartridge from bytes (copied, a copier header skipped); 0 when shorter than one bank */
int rom_set(const uint8_t *bytes, size_t len)
{
    size_t skip = len % 0x8000 == 512 ? 512 : 0;   /* (a copier header) */
    if (len - skip < 0x8000) return 0;
    uint8_t *buf = malloc(len - skip);
    if (!buf) return 0;
    memcpy(buf, bytes + skip, len - skip);
    rom_cart = buf;
    rom_size = (uint32_t)(len - skip);
    return 1;
}

/* The cartridge from the ROM file at path (rom_set); 0 when unreadable */
int rom_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = len > 0 ? malloc((size_t)len) : NULL;
    int ok = buf && fread(buf, 1, (size_t)len, f) == (size_t)len && rom_set(buf, (size_t)len);
    fclose(f);
    free(buf);
    return ok;
}
