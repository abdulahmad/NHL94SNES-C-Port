/* crowd: the hot / cold table (src/routines/crowd). */
#include "game.h"

/* At a period start (not overtime): crowd excitement $10 per playoff stage */
void InitCrowdExcite(void)
{
    if (g.gsp == 3) return;
    uint16_t r0 = 0;
    if (g.OptPlayMode != 0 && g.OptPlayMode != 4) {
        r0 = 1;
        if (!((uint16_t)(g.playoffRoundIdx - 2) & 0x8000)) {
            r0 = 2;
            if (!((uint16_t)(g.playoffRoundIdx - 3) & 0x8000)) r0 = 3;
        }
    }
    g.CwdExciteLvl = (uint16_t)(r0 << 4);
}

/* The crowd meter (d0): sqrt(CwdExciteLvl * 4) + 65, at most 128; the peak and the running average */
uint16_t Crowd_Noise_meter(void)
{
    uint16_t d0 = (uint16_t)(Sroot((uint16_t)(g.CwdExciteLvl << 2)) + 0x41);
    if (d0 >= 0x80) d0 = 0x80;
    if (!((uint16_t)(d0 - g.CrowdPeak) & 0x8000)) g.CrowdPeak = d0;
    uint32_t sum = (g.CrowdMeterSum[0] | (uint32_t)g.CrowdMeterSum[1] << 16) + d0;
    g.CrowdMeterSum[0] = (uint16_t)sum;
    g.CrowdMeterSum[1] = (uint16_t)(sum >> 16);
    g.CrowdMeterCount++;
    g.CrowdAvg = Div32U(sum, g.CrowdMeterCount);
    return d0;
}

/* The crowd meter into CurCrowdMeter: 110 dB or more sets CrowdLoud; under 105 dB a loud crowd's raised
 * hot / cold values come down */
void Crowd_Noise(void)
{
    uint16_t d0 = Crowd_Noise_meter();
    g.CurCrowdMeter = d0;
    if ((uint16_t)(d0 - 0x69) & 0x8000) {
        if (g.CrowdLoud) { LowerHotColdTable(); g.CrowdLoud = 0; }
    } else if (!((uint16_t)(d0 - 0x6E) & 0x8000)) g.CrowdLoud = 1;
}

/* While HotColdRaised is not 0: the first four bytes of each of the 52 hot / cold entries go down by 2 */
void LowerHotColdTable(void)
{
    if (!g.HotColdRaised) return;
    for (int k = 0; k < 52; k++)
        for (int b = 0; b < 4; b++) g.HotColdTable[k * 12 + b] -= 2;
    g.HotColdRaised--;
}

/* Start bench a cheering, unless one already is */
void StartBenchCheer(uint16_t a)
{
    if (g.benchcheer) return;
    g.benchcheer = a;
    g.benchframe = (uint16_t)(a << 3);
    g.benchtimer = 0x0B;
    g.benchrounds = 0;
}

/* Each frame: step the cheering bench through its frames, and end it ($FFFF) after its rounds */
void UpdateBenchCheer(void)
{
    if (!g.benchcheer) { g.benchframe = 0xFFFF; return; }
    g.benchframe &= 7;
    uint16_t t = (uint16_t)(g.benchtimer - g.timestep);
    if (!(t & 0x8000)) g.benchtimer = t;
    else {
        g.benchtimer = (uint16_t)(t + 0x0B);
        if (g.benchrounds == 4) {
            uint16_t f = (uint16_t)(g.benchframe - 1);
            if (f & 0x8000) { g.benchcheer = 0; g.benchframe = 0xFFFF; return; }
            g.benchframe = f;
        } else {
            uint16_t f = (uint16_t)(g.benchframe + 1);
            if (f != 6) g.benchframe = f;
            else if (++g.benchrounds != 4) g.benchframe = 2;
        }
    }
    g.benchframe = (uint16_t)((uint16_t)(g.benchcheer << 3) + g.benchframe + (g.benchcheer >> 13 & 1));   /* (asl's carry) */
}

/* Fill the hot / cold table (both teams, 624 bytes, from the end down) with random values -12 to 12: a
 * random 0-13 limited to 12, negative when bit 4 of the seed is clear after it */
void Create_HotCold_Table(void)
{
    for (int x = 0x26F; x >= 0; x--) {
        uint16_t r3 = randoma8(0x0E);
        if (r3 >= 0x0C) r3 = 0x0C;
        g.HotColdTable[x] = (uint8_t)(g.seed[0] & 0x10 ? r3 : -r3);
    }
}
