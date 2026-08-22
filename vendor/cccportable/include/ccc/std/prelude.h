/*
 * Concurrent-C stdlib prelude.
 *
 * Includes all stdlib headers. All types use the CC prefix (CCFile, CCArena, etc.)
 * to avoid namespace collisions - this is idiomatic C style.
 */
#ifndef CC_STD_PRELUDE_H
#define CC_STD_PRELUDE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <ccc/cc_arena.h>
#include <ccc/cc_arc.h>
#include <ccc/cc_grammar.h>
#include <ccc/cc_shape.h>
#include <ccc/cc_type.h>
#include <ccc/cc_slice.h>
#include <ccc/cc_result.h>
#include <ccc/cc_arena_result.h>
#include <ccc/cc_exclusive.h>
#include <ccc/cc_exclusive_result.h>
#include "slice.h"
#include <ccc/cc_channel.h>
#include <ccc/cc_nursery.h>
#include <ccc/cc_exec.h>
#include "string.h"
#include "slice_packed.h"
#include "io.h"
#include "vec.h"
#include <ccc/cc_turnstile.h>
#include "map_forward.h"
#include "array_map.h"
#include "shard_map.h"
#include "dir.h"
#include "process.h"
#include "exec.h"
#include "async_io.h"
#include "future.h"

/* `kilobytes`/`megabytes` moved into `ccc/cc_arena.cch` so any
 * code that allocates an arena gets them without pulling in the
 * full std prelude.  Re-included here transitively via the
 * arena import above, so existing `#include
 * <ccc/std/prelude.h>` users see no change. */

#endif // CC_STD_PRELUDE_H

