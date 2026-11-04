#!/usr/bin/env bash
# The game still plays bit for bit as it did: each script in test/scripts played headless
# (build/golden), its file of hashes a frame (work RAM; VRAM, CGRAM and OAM; the sound CPU's RAM;
# the picture; the sound) checked against test/expected.txt (ninety-four-snes's, whose C was checked
# against the cartridge in an emulator). A line of it is the hash, the frames and the run's name,
# then (when the run is not simply its script's) the script and golden's options, e.g. Game Speed Smooth.
# From the repository's root, after make: test/check.sh ROM
set -uo pipefail
cd "$(dirname "$0")/.."
rom=${1:?usage: test/check.sh ROM}
mkdir -p out
bad=0
while read -r want frames name script opts; do
    [ "${want#\#}" != "$want" ] && continue
    timeout 600 ./build/golden "$rom" --frames "$frames" --golden "out/$name.golden" --script "test/scripts/${script:-$name}.txt" $opts &
    pids+=("$!:$want:$name")
done < test/expected.txt
for p in "${pids[@]}"; do
    IFS=: read -r pid want name <<< "$p"
    wait "$pid" || { echo "$name: did not run"; bad=1; continue; }
    got=$(sha256sum < "out/$name.golden" | cut -c1-64)
    if [ "$got" = "$want" ]; then echo "$name: identical"; else echo "$name: DIFFERENT"; bad=1; fi
done
exit $bad
