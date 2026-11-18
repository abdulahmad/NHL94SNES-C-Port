## Notes

NHL '94 SNES C-port on Overbar for PortMaster (RG35XX H700 / muOS first).

This port is a derivative of [NHL94SNES](https://github.com/abdulahmad/NHL94SNES) by **abdulahmad**, with **chaos**, **StaticEck**, **McMarkis (Mhopkinsinc)** and **Drezz**, and special thanks to **smozoma** and **kingraph**. Host overlay/scaler: Overbar (PulseMan MIT). Carry this credit in any fork or release.

Private zip first. Do not upload to portmaster.games until Abdul says the catalogue is allowed (EA trademarks).

The ROM is never in the zip. Copy your US dump to `roms/nhl94.smc`. The sound CPU program is built on the compile machine from that dump (`make ROM=`), not committed.

## Controls

South = B pass/switch, east = A shoot/check, west = Y dump, north = X goalie, L/R defensemen, Start pause, Select overlay, Select+Start quit.

## Handheld

60 Hz, vsync on, overlay only, SDL_Renderer / KMSDRM fullscreen. No desktop OpenGL. Missing ROM prints to the log; the launcher never starts the binary without it (KMSDRM has no message box).

## Compile

`make ROM=/path/to/nhl94.smc` in the PortMaster aarch64 builder. Strip, rename to `nhl94s.aarch64`. `portmaster/pack.sh` assembles the zip.
