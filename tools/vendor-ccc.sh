#!/bin/sh
# Refresh vendor/cccportable from the author's ccc toolchain.
# Consumers never run this — the tree is checked in (~3MiB host-C headers+runtime).
# Parallel stays ON for everyone; cmake does not invoke ccc.
set -e
root="$(cd "$(dirname "$0")/.." && pwd)"
dir="$root/vendor/cccportable"
CCC="${CCC:-ccc}"

if [ -d "$dir" ] && [ ! -f "$dir/CCCPORTABLE.txt" ]; then
    echo "vendor-ccc.sh: $dir exists but has no CCCPORTABLE.txt — remove it first" >&2
    exit 1
fi

"$CCC" portable-install "$dir"
echo "vendor-ccc.sh: $(cat "$dir/CCCPORTABLE.txt") → $dir"
