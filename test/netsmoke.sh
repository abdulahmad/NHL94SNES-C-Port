#!/usr/bin/env bash
# Online play on one machine: two headless games (build/netsmoke, make netsmoke), the host and a
# guest on 127.0.0.1, each its own random pads and a local game of a different length first; the
# session must power both on again the same and keep them so (ob_net_check every 60 steps), and
# both must print the same hash at the last step. The guest takes the host's Game Speed (Smooth
# here) and multitap (three players). From the repository's root: test/netsmoke.sh ROM [STEPS]
set -uo pipefail
cd "$(dirname "$0")/.."
rom=${1:?usage: test/netsmoke.sh ROM [STEPS]}
steps=${2:-1200}
port=$((20000 + RANDOM % 20000))
mkdir -p out
./build/netsmoke "$rom" --host "$port" --steps "$steps" --pre 50 --seed 1 --here 2 --all 3 --delay 3 --smooth > out/net_host.txt &
h=$!
./build/netsmoke "$rom" --join "127.0.0.1:$port" --steps "$steps" --pre 300 --seed 2 --here 1 > out/net_guest.txt &
g=$!
wait $h; hs=$?
wait $g; gs=$?
cat out/net_host.txt out/net_guest.txt
[ $hs -eq 0 ] && [ $gs -eq 0 ] || { echo "netsmoke: a machine failed (host $hs, guest $gs)"; exit 1; }
a=$(cut -d' ' -f1-8 out/net_host.txt); b=$(cut -d' ' -f1-8 out/net_guest.txt)
if [ -n "$a" ] && [ "$a" = "$b" ]; then echo "netsmoke: the same"; else echo "netsmoke: DIFFERENT"; exit 1; fi
