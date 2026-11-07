/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 abdulahmad and contributors
 */
#ifndef OVERBAR_CLIENT_H
#define OVERBAR_CLIENT_H

#include <stdint.h>
#include "overbar/overbar.h"

void overbar_client_init(ob_profile profile);
void overbar_client_free(void);
ob_host *overbar_client_host(void);
int overbar_client_open(void);
int overbar_client_quit(void);
int overbar_client_stats(void);
void overbar_client_set_stats(int on);
int overbar_client_key(int scancode);           /* 1 if overlay/stats consumed it */
int overbar_client_pad_back(void);
int overbar_client_input(int mdrt_input);
int overbar_client_pointer(int x, int y, int w, int h, int what);
void overbar_client_draw(uint32_t *px, int w, int h, int pitch);
void overbar_client_view(int win_w, int win_h, int *x, int *y, int *w, int *h);

#endif
