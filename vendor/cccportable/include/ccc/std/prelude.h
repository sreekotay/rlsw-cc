/*
 * Concurrent-C stdlib prelude.
 *
 * Includes all stdlib headers. All types use the CC prefix (CCFile, CCArena, etc.)
 * to avoid namespace collisions — idiomatic C style.
 *
 * Default CCS translation units do not get libc: <string.h> names (memcpy,
 * memset, …) stay undeclared unless the user includes them. Generated zero/copy
 * uses cc__bytes_zero / cc__bytes_copy in cc_slice.cch; allocation uses arenas.
 */
#ifndef CC_STD_PRELUDE_H
#define CC_STD_PRELUDE_H

#include <stddef.h>
#include <stdint.h>

#include <ccc/cc_arena.h>
#include <ccc/cc_arc.h>
#include <ccc/cc_grammar.h>
#include <ccc/cc_shape.h>
#include <ccc/cc_type.h>
#include <ccc/cc_slice.h>
#include <ccc/cc_box.h>
#include <ccc/cc_result.h>
#include <ccc/cc_arena_result.h>
#include <ccc/cc_exclusive.h>
#include <ccc/cc_exclusive_result.h>
#include "slice.h"
#include <ccc/cc_channel.h>
#include <ccc/cc_nursery.h>
#include <ccc/cc_parallel.h>
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

#endif // CC_STD_PRELUDE_H

