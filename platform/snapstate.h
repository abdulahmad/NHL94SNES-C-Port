/* States of the running game for online play's rollback (Overbar's ob_net_states): the game's
 * memory, the console (video, DMA, pads), the clock with the sound CPU and chip, and the game's C
 * stack. The game is C that runs its own loop and calls the platform at each frame's end
 * (console_frame_done), so a state is taken there, inside that call: everything from the frame
 * that takes it (snap_mark's caller) up to console_run is copied as it is, with where to come
 * back to. Loading one puts all of it back and comes back there (snap_jump), as if the call had
 * just returned the first time. Same process, same addresses: the pointers in the frames stay
 * good. Only at a frame that ends inside the game's wait for an interrupt (clock_waiting): there
 * no routine is half run, so its statics outside the work RAM are not in use. */
#ifndef SNAPSTATE_H
#define SNAPSTATE_H
#include <stddef.h>

/* where a state taken now comes back to: snap_mark(jb) in the frame that calls the netplay core
 * (jb: void *[5], for __builtin_setjmp, set just before), with the lowest address in it */
void snap_mark(void **jb, char *low);
/* the state (ob_net_states.save: out NULL or too small gives the size) */
size_t snap_save(void *user, void *out, size_t cap);
/* ob_net_states.load: keeps the bytes for snap_jump (they stay good until the next save) */
int snap_load(void *user, const void *in, size_t size);
/* puts the state snap_load kept back and comes back where it was taken; does not return */
void snap_jump(void);
/* an address in the caller's frame, below all of it */
char *snap_sp(void);

#endif
