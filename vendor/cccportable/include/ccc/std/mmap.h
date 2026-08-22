/*
 * POSIX file map. Opt-in — not in the std prelude (Windows has no mmap).
 *
 *   CCMappedFile m = cc_file_map(path) !> @destroy;
 *   CCSlice s = m.as_slice();  // untracked; valid while mapped
 *
 * `@destroy` / cc_file_unmap run munmap. The slice is cc_slice_from_buffer
 * (no arena epoch) — copy into an arena to outlive the map.
 */
#ifndef CC_STD_MMAP_H
#define CC_STD_MMAP_H

#if defined(_WIN32)
#error "<ccc/std/mmap.h> is POSIX-only"
#endif

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ccc/cc_io_error.h>
#include <ccc/cc_result.h>
#include <ccc/cc_slice.h>
#include <ccc/cc_type.h>
#include <ccc/cc_ufcs.h>

typedef struct CCMappedFile {
    void *ptr;
    size_t len;
} CCMappedFile;

#ifndef CCResult_CCMappedFile_CCIoError_DEFINED
#define CCResult_CCMappedFile_CCIoError_DEFINED 1
/* --- CC auto-generated type declaration --- */
#ifndef CCResult_CCMappedFile_CCIoError_DEFINED
#define CCResult_CCMappedFile_CCIoError_DEFINED 1
CC_DECL_RESULT_SPEC(CCResult_CCMappedFile_CCIoError, CCMappedFile, CCIoError)
#endif
CC_DECL_RESULT_SPEC(CCResult_CCMappedFile_CCIoError, CCMappedFile, CCIoError)
#endif

static inline CCSlice cc_mapped_file_as_slice(const CCMappedFile *m) {
    if (!m || !m->ptr || m->len == 0) return cc_slice_empty();
    return cc_slice_from_buffer(m->ptr, m->len);
}

/* Idempotent: nulls ptr. No-op when already unmapped or empty. */
static inline void cc_file_unmap(CCMappedFile *m) {
    if (!m || !m->ptr || m->len == 0) {
        if (m) {
            m->ptr = NULL;
            m->len = 0;
        }
        return;
    }
    (void)munmap(m->ptr, m->len);
    m->ptr = NULL;
    m->len = 0;
}

static inline CCResult_CCMappedFile_CCIoError cc_file_map(CCSlice path) {
    CCMappedFile m = {0};
    int fd;
    struct stat st;
    void *p;
    if (!path.ptr || path.len == 0) {
        return cc_err_CCResult_CCMappedFile_CCIoError(cc_io_from_errno(EINVAL));
    }
    fd = open((const char *)path.ptr, O_RDONLY);
    if (fd < 0) return cc_err_CCResult_CCMappedFile_CCIoError(cc_io_from_errno(errno));
    if (fstat(fd, &st) != 0) {
        int e = errno;
        close(fd);
        return cc_err_CCResult_CCMappedFile_CCIoError(cc_io_from_errno(e));
    }
    if (st.st_size < 0) {
        close(fd);
        return cc_err_CCResult_CCMappedFile_CCIoError(cc_io_from_errno(EINVAL));
    }
    if (st.st_size == 0) {
        close(fd);
        return cc_ok_CCResult_CCMappedFile_CCIoError(m);
    }
    p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return cc_err_CCResult_CCMappedFile_CCIoError(cc_io_from_errno(errno));
    m.ptr = p;
    m.len = (size_t)st.st_size;
    return cc_ok_CCResult_CCMappedFile_CCIoError(m);
}



#endif /* CC_STD_MMAP_H */
