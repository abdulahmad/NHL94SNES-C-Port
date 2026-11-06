/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 abdulahmad and contributors
 *
 * SNES C-port Overbar client: dest-rect L5, overlay ` / F11, stats F12,
 * Game Speed addon, no Picture Native.
 */
#include "overbar_client.h"

#include <stddef.h>
#include "console.h"

static ob_host host;
static int speed_addon;

static int toggle_speed(void *ctx)
{
    (void)ctx;
    original_speed = !original_speed;
    return 1;
}

void overbar_client_init(ob_profile profile)
{
    ob_host_init(&host, OB_PORT_SNES, profile);
    host.addon_ctx = NULL;
    speed_addon = OB_ID_ADDON;
    ob_host_add_addon(&host, speed_addon, "Game Speed", toggle_speed);
}

void overbar_client_free(void)
{
    ob_host_free(&host);
}

ob_host *overbar_client_host(void) { return &host; }

int overbar_client_open(void) { return host.overlay_open; }
int overbar_client_quit(void) { return host.request_quit; }
int overbar_client_stats(void) { return host.stats; }

void overbar_client_set_stats(int on) { host.stats = on; }

int overbar_client_key(int scancode)
{
    if (ob_overlay_toggle_key(&host, scancode)) return 1;
    if (ob_stats_toggle_key(&host, scancode)) return 1;
    return 0;
}

int overbar_client_pad_back(void)
{
    if (host.overlay_open) ob_overlay_close(&host);
    else ob_overlay_open(&host);
    return 1;
}

int overbar_client_input(int mdrt_input)
{
    return ob_overlay_input(&host, mdrt_input);
}

void overbar_client_draw(uint32_t *px, int w, int h, int pitch)
{
    ob_overlay_draw(&host, px, w, h, pitch);
}

void overbar_client_view(int win_w, int win_h, int *x, int *y, int *w, int *h)
{
    ob_view_in in;
    ob_view_out out;
    ob_view_from_host(&host, OB_CLASS_LORES, 256, 224, win_w, win_h, &in);
    ob_view(&in, &out);
    *x = out.blit_x;
    *y = out.blit_y;
    *w = out.blit_w;
    *h = out.blit_h;
}
