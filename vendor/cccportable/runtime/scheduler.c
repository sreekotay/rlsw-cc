/*
 * Executor-backed scheduler facade with cooperative deadlines.
 */

#include <ccc/cc_sched.h>
#include <ccc/cc_exec.h>
#include <ccc/std/task.h>
#include "fiber_internal.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct CCSpawnTask {
    void* (*fn)(void*);
    void* arg;
    void* result;
    int done;
    int detached;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    /* Fiber-aware join support.
     * done_atomic mirrors done but is safe to use with cc__fiber_park_if.
     * Set with release ordering AFTER result is stored and AFTER done=1. */
    _Atomic int done_atomic;
    cc__fiber* _Atomic waiter_fiber;
};

/* Accessor to set spawn task in CCTask._data */
static inline void cc__set_spawn_task(CCTask* t, struct CCSpawnTask* task) {
    /* CCTaskSpawnInternal layout: just a pointer at offset 0 */
    struct CCSpawnTask** ptr = (struct CCSpawnTask**)t->_data;
    *ptr = task;
}

static CCExec* g_sched_exec = NULL;
static pthread_mutex_t g_sched_mu = PTHREAD_MUTEX_INITIALIZER;

static size_t cc__env_size(const char* name, size_t fallback) {
    const char* v = getenv(name);
    if (!v || !*v) return fallback;
    char* end = NULL;
    unsigned long n = strtoul(v, &end, 10);
    if (!end || end == v || *end != 0) return fallback;
    return (size_t)n;
}

static size_t cc__default_workers(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n > 0) return (size_t)n;
    return 4;
}

static CCExec* cc__sched_exec_lazy(void) {
    pthread_mutex_lock(&g_sched_mu);
    if (!g_sched_exec) {
        /* Unify with fiber scheduler: check programmatic setting first, then env */
        size_t workers = cc_sched_get_num_workers();
        if (workers == 0) {
            workers = cc__env_size("CC_WORKERS", cc__default_workers());
        }
        /* qcap=0 means "unbounded, grow on demand". cc_thread_spawn() is
         * fire-and-forget and must never silently drop a task; backpressure
         * here would also risk deadlock (e.g. a fiber that spawns thread
         * work and is the only drainer). Callers that want a hard cap can
         * set CC_SPAWN_QUEUE_CAP explicitly. */
        size_t qcap = cc__env_size("CC_SPAWN_QUEUE_CAP", 0);
        g_sched_exec = cc_exec_create(workers, qcap);
    }
    CCExec* ex = g_sched_exec;
    pthread_mutex_unlock(&g_sched_mu);
    return ex;
}

int cc_scheduler_init(void) {
    return cc__sched_exec_lazy() ? 0 : -1;
}

void cc_scheduler_shutdown(void) {
    pthread_mutex_lock(&g_sched_mu);
    if (g_sched_exec) {
        cc_exec_shutdown(g_sched_exec);
        cc_exec_free(g_sched_exec);
        g_sched_exec = NULL;
    }
    pthread_mutex_unlock(&g_sched_mu);
}

int cc_scheduler_stats(CCSchedulerStats* out) {
    if (!out) return EINVAL;
    CCExec* ex = cc__sched_exec_lazy();
    if (!ex) return ENOMEM;
    CCExecStats stats;
    int err = cc_exec_stats(ex, &stats);
    if (err != 0) return err;
    out->workers = stats.workers;
    out->queue_cap = stats.queue_cap;
    out->queue_len = stats.queue_len;
    return 0;
}

static void cc__spawn_task_free_internal(struct CCSpawnTask* task) {
    if (!task) return;
    pthread_mutex_destroy(&task->mu);
    pthread_cond_destroy(&task->cv);
    free(task);
}

static void cc__spawn_task_job(void* arg) {
    struct CCSpawnTask* task = (struct CCSpawnTask*)arg;
    if (!task) return;
    void* r = NULL;
    if (task->fn) r = task->fn(task->arg);
    pthread_mutex_lock(&task->mu);
    task->result = r;
    task->done = 1;
    /* Grab any waiting fiber under the lock (prevents it missing done=1). */
    cc__fiber* waiter = (cc__fiber*)atomic_exchange_explicit(
        &task->waiter_fiber, NULL, memory_order_acq_rel);
    /* done_atomic release store: ensures result/done are visible to fiber
     * via cc__fiber_park_if's acquire load on done_atomic. */
    atomic_store_explicit(&task->done_atomic, 1, memory_order_release);
    pthread_cond_broadcast(&task->cv);
    int detach = task->detached;
    pthread_mutex_unlock(&task->mu);
    /* Wake fiber outside lock — unpark is safe to call from any thread. */
    if (waiter) cc__fiber_unpark(waiter);
    if (detach) {
        cc__spawn_task_free_internal(task);
    }
}

/* NEW unified API: cc_thread_spawn returns CCTask value */
CCTask cc_thread_spawn(void* (*fn)(void*), void* arg) {
    CCTask out;
    memset(&out, 0, sizeof(out));
    if (!fn) return out;
    CCExec* ex = cc__sched_exec_lazy();
    if (!ex) return out;
    struct CCSpawnTask* task = (struct CCSpawnTask*)calloc(1, sizeof(struct CCSpawnTask));
    if (!task) return out;
    task->fn = fn;
    task->arg = arg;
    pthread_mutex_init(&task->mu, NULL);
    pthread_cond_init(&task->cv, NULL);
    /* Use the blocking variant so we never silently drop a task.
     *   - Default config: scheduler exec is unbounded (qcap=0), so this
     *     grows the queue on demand and effectively never blocks.
     *   - If the user explicitly sets CC_SPAWN_QUEUE_CAP, the queue is
     *     bounded and this blocks the caller — real backpressure.
     * Previously the non-blocking submit turned EAGAIN into an INVALID
     * CCTask, which cc_block_on_intptr() silently "joined" as result=0,
     * producing bogus checksums (see perf_spawn_v2_vs_thread). */
    int err = cc_exec_submit_blocking(ex, cc__spawn_task_job, task);
    if (err != 0) {
        cc__spawn_task_free_internal(task);
        return out;
    }
    out.kind = CC_TASK_KIND_SPAWN;
    cc__set_spawn_task(&out, task);
    return out;
}

/* Helper function that unpacks and calls a closure */
static void* cc__closure0_wrapper(void* arg) {
    CCClosure0* pc = (CCClosure0*)arg;
    void* result = pc->fn(pc->env);
    if (pc->drop) pc->drop(pc->env);
    free(pc);
    return result;
}

/* NEW unified API: cc_thread_spawn_closure0 returns CCTask value */
CCTask cc_thread_spawn_closure0(CCClosure0 c) {
    /* Wrap closure in a simple function that calls the closure */
    /* For simplicity, we allocate and pass the closure as arg */
    CCTask out;
    memset(&out, 0, sizeof(out));
    if (!c.fn) return out;
    
    /* Create a heap copy of the closure */
    CCClosure0* heap_c = (CCClosure0*)malloc(sizeof(CCClosure0));
    if (!heap_c) return out;
    *heap_c = c;
    
    return cc_thread_spawn(cc__closure0_wrapper, heap_c);
}

/* Legacy API for backward compatibility */
int cc_thread_spawn_legacy(struct CCSpawnTask** out_task, void* (*fn)(void*), void* arg) {
    if (!out_task || !fn) return EINVAL;
    *out_task = NULL;
    CCExec* ex = cc__sched_exec_lazy();
    if (!ex) return ENOMEM;
    struct CCSpawnTask* task = (struct CCSpawnTask*)calloc(1, sizeof(struct CCSpawnTask));
    if (!task) return ENOMEM;
    task->fn = fn;
    task->arg = arg;
    pthread_mutex_init(&task->mu, NULL);
    pthread_cond_init(&task->cv, NULL);
    int err = cc_exec_submit(ex, cc__spawn_task_job, task);
    if (err != 0) {
        cc__spawn_task_free_internal(task);
        return err;
    }
    *out_task = task;
    return 0;
}

int cc_thread_task_join(struct CCSpawnTask* task) {
    if (!task) return EINVAL;
    pthread_mutex_lock(&task->mu);
    while (!task->done) {
        pthread_cond_wait(&task->cv, &task->mu);
    }
    pthread_mutex_unlock(&task->mu);
    return 0;
}

int cc_thread_task_join_result(struct CCSpawnTask* task, void** out_result) {
    if (!task) return EINVAL;
    pthread_mutex_lock(&task->mu);
    while (!task->done) {
        pthread_cond_wait(&task->cv, &task->mu);
    }
    if (out_result) *out_result = task->result;
    pthread_mutex_unlock(&task->mu);
    return 0;
}

/* Strict predicate wait for fiber context:
 * keep parking until *done becomes non-zero, tolerating unrelated wakeups. */
static inline void cc__fiber_wait_until_done(_Atomic int* done, const char* reason) {
    while (atomic_load_explicit(done, memory_order_acquire) == 0) {
        cc__fiber_clear_pending_unpark();
        CC_FIBER_PARK_IF(done, 0, reason);
    }
}

/* Fiber-aware join: park the calling fiber instead of blocking the worker thread.
 * Must only be called from within a fiber context (cc__fiber_in_context() == 1).
 *
 * Protocol:
 *   1. Lock mutex, check done — fast path if already done.
 *   2. Register current fiber as waiter (under lock, so completion can't race past it).
 *   3. Unlock, then park on done_atomic via cc__fiber_park_if.
 *      If completion fires between unlock and park, pending_unpark or the flag
 *      check in cc__fiber_park_if prevents the park.
 *   4. On wakeup done_atomic==1, result is visible via release/acquire ordering. */
int cc_thread_task_join_fiber(struct CCSpawnTask* task, void** out_result) {
    if (!task) return EINVAL;
    pthread_mutex_lock(&task->mu);
    if (!task->done) {
        /* Register as waiter under lock so the completion handler can see us. */
        atomic_store_explicit(&task->waiter_fiber,
                              (cc__fiber*)cc__fiber_current(),
                              memory_order_relaxed);
        pthread_mutex_unlock(&task->mu);
        /* Park until done_atomic is set.
         *
         * IMPORTANT: clear stale pending_unpark before each park attempt.
         * Otherwise an unrelated wake can cause cc__fiber_park_if() to return
         * immediately while done_atomic is still 0, and we'd read task->result
         * before the spawn job publishes it. */
        cc__fiber_wait_until_done(&task->done_atomic, "spawn_join");
        /* Clear stale waiter registration in case park bailed on flag or
         * pending_unpark without the completion handler clearing it. */
        atomic_store_explicit(&task->waiter_fiber, NULL, memory_order_relaxed);
    } else {
        pthread_mutex_unlock(&task->mu);
    }
    /* done_atomic acquire (in park_if or flag check) pairs with the release store
     * in cc__spawn_task_job, so task->result is visible here. */
    if (out_result) *out_result = task->result;
    return 0;
}

void cc_thread_task_free(struct CCSpawnTask* task) {
    if (!task) return;
    pthread_mutex_lock(&task->mu);
    if (task->done) {
        pthread_mutex_unlock(&task->mu);
        cc__spawn_task_free_internal(task);
        return;
    }
    task->detached = 1;
    pthread_mutex_unlock(&task->mu);
}

/* Non-blocking poll: check if thread task is done without blocking */
int cc_thread_task_poll_done(struct CCSpawnTask* task) {
    if (!task) return 0;
    pthread_mutex_lock(&task->mu);
    int done = task->done;
    pthread_mutex_unlock(&task->mu);
    return done;
}

/* Get result from a completed thread task (caller must ensure task is done) */
void* cc_thread_task_get_result(struct CCSpawnTask* task) {
    if (!task) return NULL;
    pthread_mutex_lock(&task->mu);
    void* result = task->result;
    pthread_mutex_unlock(&task->mu);
    return result;
}

int cc_sleep_ms(unsigned int ms) {
    /* Fiber-aware: park the fiber on the sleep queue with a deadline.
     * Sysmon drains expired sleepers every ~250µs and re-enqueues them.
     * This avoids O(N) queue churn when many fibers sleep concurrently. */
    if (cc__fiber_in_context()) {
        cc__fiber_sleep_park(ms);
        return 0;
    }
    /* Thread context: block the OS thread directly. */
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        continue;
    }
    return 0;
}

CCDeadline cc_deadline_none(void) {
    CCDeadline d; d.deadline.tv_sec = 0; d.deadline.tv_nsec = 0; d.cancelled = 0; return d; }

CCDeadline cc_deadline_after_ms(uint64_t ms) {
    CCDeadline d = cc_deadline_none();
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    uint64_t nsec = (uint64_t)now.tv_nsec + (ms % 1000) * 1000000ULL;
    d.deadline.tv_sec = now.tv_sec + (time_t)(ms / 1000) + (time_t)(nsec / 1000000000ULL);
    d.deadline.tv_nsec = (long)(nsec % 1000000000ULL);
    return d;
}

bool cc_deadline_expired(const CCDeadline* d) {
    if (!d || d->cancelled) return true;
    if (d->deadline.tv_sec == 0) return false;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (now.tv_sec > d->deadline.tv_sec) return true;
    if (now.tv_sec == d->deadline.tv_sec && now.tv_nsec >= d->deadline.tv_nsec) return true;
    return false;
}

/* Undefine language-level macros to expose the CCDeadline*-taking API. */
#undef cc_cancel
#undef cc_is_cancelled
void cc_cancel(CCDeadline* d) { if (d) d->cancelled = 1; }
bool cc_is_cancelled(const CCDeadline* d) { return d && d->cancelled; }

const struct timespec* cc_deadline_as_timespec(const CCDeadline* d, struct timespec* out) {
    if (!d || d->deadline.tv_sec == 0) return NULL;
    if (out) *out = d->deadline;
    return out;
}

static _Atomic int g_par_live;
static _Atomic int g_par_cap;

static int cc_parallel_cap(void) {
    int cap = atomic_load_explicit(&g_par_cap, memory_order_relaxed);
    if (cap > 0) return cap;
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;
    if (n > 4096) n = 4096;
    cap = (int)n * 256;
    atomic_store_explicit(&g_par_cap, cap, memory_order_relaxed);
    return cap;
}

size_t sched_v2_ready_depth(void);

/* Backlog-keyed spawn denial. When the scheduler's ready queue already
 * holds this many runnable fibers, a further @parallel spawn adds
 * queue/lock traffic but no parallelism — no worker would reach it soon.
 * Denying instead makes the lowering run the arm inline on the caller
 * (the documented spawn-failure fallback: no task record, no enqueue, no
 * join). Workers keep draining the retained backlog in parallel, so
 * fine-grained spawn storms self-limit to roughly the threshold.
 *
 * Default off. Queue depth cannot distinguish churn from legitimate
 * arms waiting their turn: perf/parallel_steal_probe.ccs shows any
 * fixed threshold serializes a well-grained @parallel for whenever the
 * queue is busy (64 arms x 3ms ran fully serial under a concurrent
 * spawn storm, and lost half their speedup even solo), while ungated
 * FIFO keeps real work parallel because churn entries drain in
 * microseconds. The gate only wins when the entire workload is churn
 * (perf/parallel_hello.ccs ungated: ~16x faster, ~32x smaller RSS), so
 * it stays an opt-in knob for such workloads. -1 sentinel = unread. */
static _Atomic int g_par_backlog_cached = -1;

static int cc_parallel_spawn_backlog(void) {
    int v = atomic_load_explicit(&g_par_backlog_cached, memory_order_relaxed);
    if (v >= 0) return v;
    const char* s = getenv("CC_PAR_SPAWN_BACKLOG");
    int newv = 0;
    if (s && s[0]) {
        long n = strtol(s, NULL, 10);
        if (n > 0 && n <= 1000000L) newv = (int)n;
    }
    atomic_store_explicit(&g_par_backlog_cached, newv, memory_order_relaxed);
    return newv;
}

/* CC_PAR_NOCAP=1 (diagnostic): skip the in-flight cap accounting entirely
 * — isolates the cost of the two shared-cacheline RMWs per spawn. */
static _Atomic int g_par_nocap_cached = -1;

static int cc_parallel_nocap(void) {
    int v = atomic_load_explicit(&g_par_nocap_cached, memory_order_relaxed);
    if (v >= 0) return v;
    const char* s = getenv("CC_PAR_NOCAP");
    int newv = (s && s[0] && s[0] != '0') ? 1 : 0;
    atomic_store_explicit(&g_par_nocap_cached, newv, memory_order_relaxed);
    return newv;
}

/* ============================================================================
 * Duration-adaptive spawn gating (CC_PAR_ADAPT, default on).
 *
 * The fixed backlog gate above serializes legitimate work because queue
 * depth cannot distinguish a million 1us churn entries from 64 real 3ms
 * arms. Arm DURATION can — spawning an arm cheaper than the spawn+join
 * round trip (~1.5us) is a pure loss, spawning a multi-ms arm is always a
 * win — and duration is measurable per call site: the lowering passes each
 * @parallel construct's thunk, a stable per-construct function pointer.
 *
 * Per site: the first CC_PAR_LEARN spawns run through a timing trampoline
 * (two clock reads per arm, only while learning). Once classified:
 *   - churn (mean arm < threshold): deny the spawn whenever the ready
 *     queue is non-trivial. The lowering runs the denied arm inline on
 *     the caller — nothing is queued, so denied work can never strand,
 *     and inline is faster than spawn for such arms anyway. A denied
 *     subtree that turns its fiber into a long serial run parks no work:
 *     if it kidnaps its worker, sysmon's eviction sees the queue backlog
 *     and staffs a replacement — recovery is sysmon's job; the gate's
 *     only duty is that admitted work is always globally visible.
 *   - real (mean arm >= threshold): never denied; steady-state spawns
 *     run unwrapped at zero added cost.
 *
 * Classification is sticky: measurement stops after learning, so a site
 * whose arms later grow much heavier keeps being inlined when the queue
 * is busy (bounded loss: it degrades toward sequential execution of that
 * construct, never stranded work). Revisit with resampling if a real
 * workload hits this; CC_PAR_ADAPT=0 opts out wholesale.
 *
 * Table: fixed-size open-addressed, fn-keyed, insert-only. On overflow
 * new sites stay unclassified and simply spawn — the pre-gate behavior. */
#define CC_PAR_SITE_SLOTS 256u /* power of two */
#define CC_PAR_LEARN      8u

typedef struct {
    _Atomic(void*)   fn;
    _Atomic uint64_t sum_ns;  /* total measured arm time while learning */
    _Atomic uint32_t samples; /* completed measured arms */
} cc_par_site;

static cc_par_site g_par_sites[CC_PAR_SITE_SLOTS];

static _Atomic int g_par_adapt_cached = -1;      /* CC_PAR_ADAPT, default 1 */
static _Atomic long g_par_churn_ns_cached = -1;  /* CC_PAR_CHURN_NS */
static _Atomic int g_par_adapt_backlog_cached = -1; /* CC_PAR_ADAPT_BACKLOG */

static int cc_parallel_adapt_on(void) {
    int v = atomic_load_explicit(&g_par_adapt_cached, memory_order_relaxed);
    if (v >= 0) return v;
    const char* s = getenv("CC_PAR_ADAPT");
    int newv = (s && s[0] == '0') ? 0 : 1;
    atomic_store_explicit(&g_par_adapt_cached, newv, memory_order_relaxed);
    return newv;
}

/* Churn threshold: mean arm duration below this classifies the site as
 * churn. Default 32us ~ 20x the measured spawn+join round trip, wide of
 * both edges: hello-style tree arms measure ~1-2us, the steal probe's
 * legitimate arms 3ms. */
static uint64_t cc_parallel_churn_ns(void) {
    long v = atomic_load_explicit(&g_par_churn_ns_cached, memory_order_relaxed);
    if (v >= 0) return (uint64_t)v;
    const char* s = getenv("CC_PAR_CHURN_NS");
    long newv = 32000;
    if (s && s[0]) {
        long n = strtol(s, NULL, 10);
        if (n >= 0 && n <= 1000000000L) newv = n;
    }
    atomic_store_explicit(&g_par_churn_ns_cached, newv, memory_order_relaxed);
    return (uint64_t)newv;
}

/* Queue depth at which a churn site's spawns are denied. The empirical
 * sweet spot from the backlog sweep (196ms/4MB at depth 24); anything
 * 2..8 measured within noise of each other. */
static int cc_parallel_adapt_backlog(void) {
    int v = atomic_load_explicit(&g_par_adapt_backlog_cached,
                                 memory_order_relaxed);
    if (v >= 0) return v;
    const char* s = getenv("CC_PAR_ADAPT_BACKLOG");
    int newv = 4;
    if (s && s[0]) {
        long n = strtol(s, NULL, 10);
        if (n > 0 && n <= 1000000L) newv = (int)n;
    }
    atomic_store_explicit(&g_par_adapt_backlog_cached, newv,
                          memory_order_relaxed);
    return newv;
}

static cc_par_site* cc_par_site_get(void* (*fn)(void*)) {
    uintptr_t h = (uintptr_t)fn;
    h ^= h >> 17;
    h *= 0x9E3779B97F4A7C15ull;
    uint32_t idx = (uint32_t)(h >> 32) & (CC_PAR_SITE_SLOTS - 1u);
    for (uint32_t k = 0; k < 8; k++) {
        cc_par_site* s = &g_par_sites[(idx + k) & (CC_PAR_SITE_SLOTS - 1u)];
        void* cur = atomic_load_explicit(&s->fn, memory_order_acquire);
        if (cur == (void*)fn) return s;
        if (cur == NULL) {
            void* expect = NULL;
            if (atomic_compare_exchange_strong_explicit(
                    &s->fn, &expect, (void*)fn,
                    memory_order_acq_rel, memory_order_acquire))
                return s;
            if (expect == (void*)fn) return s;
        }
    }
    return NULL; /* probe window full: stay unclassified, always spawn */
}

static uint64_t cc_par_now_ns(void) {
#if defined(__APPLE__)
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

typedef struct {
    void* (*fn)(void*);
    void*        arg;
    cc_par_site* site;
} cc_par_timed;

static void* cc_par_timed_run(void* p) {
    cc_par_timed w = *(cc_par_timed*)p;
    free(p);
    uint64_t t0 = cc_par_now_ns();
    void* r = w.fn(w.arg);
    uint64_t dt = cc_par_now_ns() - t0;
    atomic_fetch_add_explicit(&w.site->sum_ns, dt, memory_order_relaxed);
    atomic_fetch_add_explicit(&w.site->samples, 1, memory_order_relaxed);
    return r;
}

CCTask cc_parallel_spawn(void* (*fn)(void*), void* arg) {
    CCTask invalid;
    memset(&invalid, 0, sizeof(invalid));
    if (!fn) return invalid;
    if (cc_parallel_nocap())
        return cc_fiber_spawn_task(fn, arg);
    /* Fixed backlog gate first (opt-in override; see its comment). The
     * denied path stays RMW-free (one relaxed load of the queue depth),
     * so a spawn storm running mostly inline doesn't hammer g_par_live. */
    int backlog = cc_parallel_spawn_backlog();
    if (backlog > 0 && sched_v2_ready_depth() >= (size_t)backlog)
        return invalid;

    /* Adaptive gate: deny churn-classified sites when the queue is busy;
     * route still-learning sites through the timing trampoline. */
    cc_par_site* site = NULL;
    if (cc_parallel_adapt_on() && (site = cc_par_site_get(fn)) != NULL) {
        uint32_t n = atomic_load_explicit(&site->samples,
                                          memory_order_relaxed);
        if (n >= CC_PAR_LEARN) {
            uint64_t sum = atomic_load_explicit(&site->sum_ns,
                                                memory_order_relaxed);
            if (sum / n < cc_parallel_churn_ns() &&
                sched_v2_ready_depth() >=
                    (size_t)cc_parallel_adapt_backlog())
                return invalid;
            site = NULL; /* classified: spawn unwrapped */
        }
        /* else: learning — wrap below (site stays set) */
    } else {
        site = NULL;
    }

    void* (*spawn_fn)(void*) = fn;
    void* spawn_arg = arg;
    cc_par_timed* w = NULL;
    if (site) {
        w = (cc_par_timed*)malloc(sizeof(*w));
        if (w) {
            w->fn = fn;
            w->arg = arg;
            w->site = site;
            spawn_fn = cc_par_timed_run;
            spawn_arg = w;
        }
    }

    int prev = atomic_fetch_add_explicit(&g_par_live, 1, memory_order_acq_rel);
    if (prev >= cc_parallel_cap()) {
        atomic_fetch_sub_explicit(&g_par_live, 1, memory_order_acq_rel);
        free(w);
        return invalid;
    }
    CCTask t = cc_fiber_spawn_task(spawn_fn, spawn_arg);
    if (t.kind == CC_TASK_KIND_INVALID) {
        atomic_fetch_sub_explicit(&g_par_live, 1, memory_order_acq_rel);
        free(w);
    }
    return t;
}

void cc_parallel_join(CCTask t) {
    (void)cc_block_on_intptr(t);
    if (!cc_parallel_nocap())
        atomic_fetch_sub_explicit(&g_par_live, 1, memory_order_acq_rel);
}

