#!/bin/bash
# PortMaster launcher for NHL '94 SNES (Overbar host, SDL_Renderer / KMSDRM).
# User-supplied dump only. Never ships a ROM.

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/nhl94s"
CONFDIR="$GAMEDIR/conf"
BIN="nhl94s.${DEVICE_ARCH}"

mkdir -p "$CONFDIR"
cd "$GAMEDIR" || exit 1

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export XDG_DATA_HOME="$CONFDIR"
export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export OVERBAR_HANDHELD=1
export PORTMASTER=1

# The game finds its ROM by itself: any file next to the program with a common extension (any name), checked to be the right game.
FOUND=""
for f in *.smc *.sfc *.swc *.fig; do [ -f "$f" ] && FOUND="$f" && break; done
if [ -z "$FOUND" ]; then
  pm_message "Copy your NHL '94 SNES dump into the port folder (any file name)."
  pm_finish
  exit 1
fi

if [ ! -x "$GAMEDIR/$BIN" ]; then
  pm_message "Missing $BIN. Cross-compile the SNES C-port for aarch64 and copy it here."
  pm_finish
  exit 1
fi

# gptokeyb: quit combo only. L/R and face buttons stay native SDL GameController.
$GPTOKEYB "$BIN" -c "$GAMEDIR/nhl94s.gptk" &
pm_platform_helper "$GAMEDIR/$BIN"
./"$BIN"
pm_finish
