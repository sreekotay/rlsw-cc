/*
 * Close libc copy / heap names in a CCS TU. Include last, after every
 * header that may have pulled string.h / stdlib.h.
 *
 * Dest-bulk (`dst.copy` / `move` / `fill`) and the arena are the APIs.
 * Gap tests that must call memcpy / malloc define CC_ALLOW_LIBC_MEM
 * before this header.
 */
#ifndef CC_NO_LIBC_MEM_H
#define CC_NO_LIBC_MEM_H

#if !defined(CC_ALLOW_LIBC_MEM) && defined(__GNUC__)
#ifdef memcpy
#undef memcpy
#endif
#ifdef malloc
#undef malloc
#endif
#pragma GCC poison memcpy malloc
#endif

#endif
