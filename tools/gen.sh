#!/bin/sh
# Regenerate checked-in Concurrent-C emits. cmake never runs this.
#   ./tools/gen.sh          # both
#   ./tools/gen.sh kernels  # span_kernels only
#   ./tools/gen.sh bin_par  # bin_par only
#
# --no-line: no absolute #line paths in the foreign/checked-in tree.
# Kernels: #pragma(@prelude) off + CC_PARSER_MODE host.h (inert in rlsw.h).
set -e
cd "$(dirname "$0")/../src/fill"
CCC="${CCC:-ccc}"
out=../../include/generated

gen_kernels() {
    "$CCC" --emit-c-only --no-runtime --no-cache --no-line --out-dir "$out" span_kernels.ccs
}

gen_bin_par() {
    "$CCC" --emit-c-only --no-runtime --no-cache --no-line --out-dir "$out" bin_par.ccs
}

case "${1:-all}" in
    all)     gen_kernels; gen_bin_par ;;
    kernels) gen_kernels ;;
    bin_par) gen_bin_par ;;
    *) echo "usage: $0 [all|kernels|bin_par]" >&2; exit 1 ;;
esac
