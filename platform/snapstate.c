/* States of the running game for online play's rollback (snapstate.h). */
#include "snapstate.h"

#include <stdint.h>
#include <string.h>
#if defined(_WIN32)
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include "game.h"
#include "mem.h"
#include "snes.h"
#include "console.h"

#define NOINLINE __attribute__((noinline))
#define MAGIC 0x53543934u                 /* "94TS" */

static void **mark_jb;
static char *mark_low;
static const uint8_t *kept;
static size_t kept_size;

void snap_mark(void **jb, char *low) { mark_jb = jb; mark_low = low; }

NOINLINE char *snap_sp(void)
{
    return (char *)__builtin_frame_address(0) - 256;   /* (this frame's base: the caller's is all above it) */
}

typedef struct head {
    uint32_t magic;
    uint32_t clock, hal;
    void *jb[5];
    void **jb_at;
    char *low;
    size_t stack;
} head;

static size_t total(size_t stack) { return sizeof(head) + sizeof g + sizeof machine + clock_save(NULL) + hal_save(NULL) + 2 + stack; }

size_t snap_save(void *user, void *out, size_t cap)
{
    (void)user;
    if (!mark_jb || !mark_low || !console_stack_top || mark_low >= console_stack_top) return 0;
    size_t stack = (size_t)(console_stack_top - mark_low), need = total(stack);
    if (!out || cap < need) return need;
    uint8_t *o = out;
    head h;
    memset(&h, 0, sizeof h);
    h.magic = MAGIC;
    h.clock = (uint32_t)clock_save(NULL);
    h.hal = (uint32_t)hal_save(NULL);
    memcpy(h.jb, mark_jb, sizeof h.jb);
    h.jb_at = mark_jb;
    h.low = mark_low;
    h.stack = stack;
    memcpy(o, &h, sizeof h); o += sizeof h;
    memcpy(o, &g, sizeof g); o += sizeof g;
    memcpy(o, &machine, sizeof machine); o += sizeof machine;
    o += clock_save(o);
    o += hal_save(o);
    *o++ = cf;
    *o++ = nmi_b;
    memcpy(o, mark_low, stack);
    return need;
}

int snap_load(void *user, const void *in, size_t size)
{
    head h;
    (void)user;
    if (size < sizeof h) return -1;
    memcpy(&h, in, sizeof h);
    if (h.magic != MAGIC || h.clock != clock_save(NULL) || h.hal != hal_save(NULL) || size != total(h.stack)
        || h.low + h.stack != console_stack_top) return -1;
    kept = in;
    kept_size = size;
    return 0;
}

/* (below every byte the state's stack puts back) */
NOINLINE static void put_back(void)
{
    head h;
    const uint8_t *i = kept;
    memcpy(&h, i, sizeof h); i += sizeof h;
    memcpy(&g, i, sizeof g); i += sizeof g;
    memcpy(&machine, i, sizeof machine); i += sizeof machine;
    clock_load(i); i += h.clock;
    hal_load(i); i += h.hal;
    cf = *i++;
    nmi_b = *i++;
    memcpy(h.low, i, h.stack);            /* (the frames, as they were) */
    memcpy(h.jb_at, h.jb, sizeof h.jb);
    __builtin_longjmp(h.jb_at, 1);
}

NOINLINE static void put_back_below(char *low)
{
    char *sp = snap_sp();
    if (sp > low - 4096) {                /* (the frames to put back reach down past here: go below them first) */
        size_t more = (size_t)(sp - (low - 4096));
        volatile char *pad = alloca(more);
        pad[0] = 0;
        put_back();
    }
    put_back();
}

void snap_jump(void)
{
    head h;
    memcpy(&h, kept, sizeof h);
    put_back_below(h.low);
}
