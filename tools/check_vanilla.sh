#!/usr/bin/env bash
# The vanilla gate (docs/VANILLA.md): the game's own code stays the original's, the host stays outside it.
#   tools/check_vanilla.sh [BASE]     BASE: the git ref the change is compared with (default origin/main)
# Fails when
#   1. game/ includes a host header (platform/'s console, menu, pad setup, settings) or SDL, or reads a setting;
#   2. the change edits test/expected.txt (a changed hash is a changed game) without a line in docs/RULES.md.
set -uo pipefail
cd "$(dirname "$0")/.."
base=${1:-origin/main}
bad=0
if grep -rnE '#include *[<"](SDL|console\.h|menu\.h|padsetup\.h|settings\.h|fnsyms\.h|platform/)' game; then
    echo "vanilla: game/ includes host headers (above)"; bad=1
fi
if grep -rnwE 'settings_[a-z_]+|getenv' game; then
    echo "vanilla: game/ reads a setting (above)"; bad=1
fi
if git rev-parse -q --verify "$base" >/dev/null; then
    changed=$(git diff --name-only "$base" -- test/expected.txt docs/RULES.md)
    if grep -qx test/expected.txt <<< "$changed" && ! grep -qx docs/RULES.md <<< "$changed"; then
        echo "vanilla: test/expected.txt changed since $base without a docs/RULES.md line saying why"; bad=1
    fi
else
    echo "vanilla: no $base here; the expected.txt check was skipped"
fi
[ $bad = 0 ] && echo "vanilla: PASS"
exit $bad
