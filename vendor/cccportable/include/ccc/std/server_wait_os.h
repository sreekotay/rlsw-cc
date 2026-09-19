/*
 * Host-only wait backend headers for <ccc/std/server_poll.ccs>.
 *
 * Keep these includes out of the .ccs/.cch text: the module face collects
 * every `#include <…>` line unconditionally, so a Darwin build would see
 * `<sys/epoll.h>` even inside `#elif CC_WAIT_USE_EPOLL`. This .h is passed
 * through to the host compiler, which evaluates the platform `#if`s.
 *
 * Override: -DCC_SERVER_WAIT_POLL=1 / _KQUEUE=1 / _EPOLL=1.
 */
#ifndef CC_STD_SERVER_WAIT_OS_H
#define CC_STD_SERVER_WAIT_OS_H

#if defined(CC_SERVER_WAIT_POLL) && CC_SERVER_WAIT_POLL
#define CC_WAIT_USE_POLL 1
#elif defined(CC_SERVER_WAIT_KQUEUE) && CC_SERVER_WAIT_KQUEUE
#define CC_WAIT_USE_KQUEUE 1
#elif defined(CC_SERVER_WAIT_EPOLL) && CC_SERVER_WAIT_EPOLL
#define CC_WAIT_USE_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) \
    || defined(__OpenBSD__) || defined(__DragonFly__)
#define CC_WAIT_USE_KQUEUE 1
#elif defined(__linux__)
#define CC_WAIT_USE_EPOLL 1
#else
#define CC_WAIT_USE_POLL 1
#endif

#if CC_WAIT_USE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#ifndef CC_SERVER_KEVENT_BATCH
#define CC_SERVER_KEVENT_BATCH 1024
#endif
#elif CC_WAIT_USE_EPOLL
#include <sys/epoll.h>
#ifndef CC_SERVER_EPOLL_BATCH
#define CC_SERVER_EPOLL_BATCH 1024
#endif
#else
#include <poll.h>
#endif

#endif /* CC_STD_SERVER_WAIT_OS_H */
