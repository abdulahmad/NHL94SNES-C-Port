/* period: periods (src/routines/period). */
#include "game.h"

/* The team rating of team a2: byte 4 of its scouting report. */
uint16_t GetTeamRating(uint16_t a2)
{
    uint32_t bank = SYM_TeamList >> 16;
    uint16_t t = r16(RAM_OFF_tmdata + a2);
    t = (uint16_t)(m16(((bank << 16 | t) + 8) & 0xFFFFFF) + t + 4);
    return m8(bank << 16 | t);
}


/* Clear the text screen and draw the horizontal rink's scoreboard background: the screen erased
 * between its two print positions, the two halves of the rink map (rinkmapbuf, columns 0 and $10
 * of row $55, 32 by 28) to the screen maps at $0400 and $0800, then USBoard. */
void SetHor_rink(void)
{
    print(SYM_StatsReportPos);
    eraser(0x20, 0x1D, g.blankchar);
    print(SYM_StatsReportPos2);
    g.MapVRAM = 0x400;
    MapPartToVRAM(SYM_rinkmapbuf, 0, 0, 0, 0x55, 0x20, 0x1C);
    g.MapVRAM = 0x800;
    MapPartToVRAM(SYM_rinkmapbuf, 0, 0, 0x10, 0x55, 0x20, 0x1C);
    USBoard();
}
