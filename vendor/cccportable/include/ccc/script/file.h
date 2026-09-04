/*
 * Script file helpers: read/copy/print by path.
 * Path arguments are CCSlice (NUL-terminated borrows).
 */
#ifndef CC_SCRIPT_FILE_H
#define CC_SCRIPT_FILE_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <ccc/cc_arena.h>
#include <ccc/cc_io_error.h>
#include <ccc/cc_result.h>
#include <ccc/cc_slice.h>
#include <ccc/std/io.h>

/* Read entire file at path into arena. Arena is last. */
/* --- CC auto-generated type declaration --- */
#ifndef CCResult_CCSlice_CCError_DEFINED
#define CCResult_CCSlice_CCError_DEFINED 1
CC_DECL_RESULT_SPEC(CCResult_CCSlice_CCError, CCSlice, CCError)
#endif
static inline CCResult_CCSlice_CCError cc_file_read_path(CCSlice path, CCArena arena) {
    if (!path.ptr || !cc_arena_is_live(arena)) {
        return cc_err_CCResult_CCSlice_CCError(CC_ERROR(CC_ERR_INVALID_ARG, "cc_file_read_path: bad args"));
    }
    CCFile file =({ CCResult_CCFile_CCIoError __cc_pu_x_1 = (cc_file_open(path));
    if (CCResult_CCFile_CCIoError_is_err(__cc_pu_x_1)) {
        cc_rt_diag_record_unwrap_site("/Users/airm5/Documents/code/concurrent-c/cc/include/ccc/script/file.cch", "23"); CCIoError __cc_pu_bind_1_e = (__cc_pu_x_1).u.error; 
        return cc_err_CCResult_CCSlice_CCError(CC_ERROR(CC_ERR_IO, cc_io_error_str(__cc_pu_bind_1_e)));
     } CCResult_CCFile_CCIoError_unwrap(__cc_pu_x_1); });
/*CC_LN 26 include/ccc/script/file.cch*/
    CCSlice data =({ CCResult_CCSlice_CCIoError __cc_pu_x_2 = (cc_file_read_all(&file, arena));
    if (CCResult_CCSlice_CCIoError_is_err(__cc_pu_x_2)) {
        cc_rt_diag_record_unwrap_site("/Users/airm5/Documents/code/concurrent-c/cc/include/ccc/script/file.cch", "26"); CCIoError __cc_pu_bind_2_e = (__cc_pu_x_2).u.error; 
        cc_file_close(&file);
        return cc_err_CCResult_CCSlice_CCError(CC_ERROR(CC_ERR_IO, cc_io_error_str(__cc_pu_bind_2_e)));
     } CCResult_CCSlice_CCIoError_unwrap(__cc_pu_x_2); });
/*CC_LN 30 include/ccc/script/file.cch*/
    cc_file_close(&file);
    return cc_ok_CCResult_CCSlice_CCError(data);
}

static inline CCResult_size_t_CCError cc_file_write_path(CCSlice path, CCSlice data) {
    if (!path.ptr) {
        return cc_err_CCResult_size_t_CCError(CC_ERROR(CC_ERR_INVALID_ARG, "cc_file_write_path: bad path"));
    }
    CCFile file = {0};{ CCResult_void_CCIoError __cc_pu_s_1 = (cc_file_create(&file, path));
    if (CCResult_void_CCIoError_is_err(__cc_pu_s_1)) {
        cc_rt_diag_record_unwrap_site("/Users/airm5/Documents/code/concurrent-c/cc/include/ccc/script/file.cch", "39"); CCIoError __cc_pu_bind_1_e = (__cc_pu_s_1).u.error; 
        return cc_err_CCResult_size_t_CCError(CC_ERROR(CC_ERR_IO, cc_io_error_str(__cc_pu_bind_1_e)));
     } }
/*CC_LN 42 include/ccc/script/file.cch*/
    size_t n =({ CCResult_size_t_CCIoError __cc_pu_x_3 = (cc_file_write(&file, data));
    if (CCResult_size_t_CCIoError_is_err(__cc_pu_x_3)) {
        cc_rt_diag_record_unwrap_site("/Users/airm5/Documents/code/concurrent-c/cc/include/ccc/script/file.cch", "42"); CCIoError __cc_pu_bind_3_e = (__cc_pu_x_3).u.error; 
        cc_file_close(&file);
        return cc_err_CCResult_size_t_CCError(CC_ERROR(CC_ERR_IO, cc_io_error_str(__cc_pu_bind_3_e)));
     } CCResult_size_t_CCIoError_unwrap(__cc_pu_x_3); });
/*CC_LN 46 include/ccc/script/file.cch*/
    cc_file_close(&file);
    return cc_ok_CCResult_size_t_CCError(n);
}

static inline CCResult_bool_CCError cc_file_copy(CCSlice src, CCSlice dst, CCArena arena) {
    CCResult_CCSlice_CCError rr;
    CCResult_size_t_CCError wr;
    if (!cc_arena_is_live(arena) || !src.ptr || !dst.ptr) {
        return cc_err_CCResult_bool_CCError(CC_ERROR(CC_ERR_INVALID_ARG, "cc_file_copy: bad args"));
    }
    rr = cc_file_read_path(src, arena);
    if (cc_is_err(rr)) {
        return cc_err_CCResult_bool_CCError(cc_error(rr));
    }
    wr = cc_file_write_path(dst, cc_value(rr));
    if (cc_is_err(wr)) {
        return cc_err_CCResult_bool_CCError(cc_error(wr));
    }
    return cc_ok_CCResult_bool_CCError(true);
}

static inline CCResult_size_t_CCError cc_script_print_file(CCSlice path, CCArena arena) {
    CCResult_CCSlice_CCError rr;
    CCResult_size_t_CCIoError wr;
    if (!cc_arena_is_live(arena) || !path.ptr) {
        return cc_err_CCResult_size_t_CCError(CC_ERROR(CC_ERR_INVALID_ARG, "cc_script_print_file: bad args"));
    }
    rr = cc_file_read_path(path, arena);
    if (cc_is_err(rr)) return cc_err_CCResult_size_t_CCError(cc_error(rr));
    wr = cc_std_out_write(cc_value(rr));
    if (cc_is_err(wr)) {
        return cc_err_CCResult_size_t_CCError(CC_ERROR(CC_ERR_IO, cc_io_error_str(cc_error(wr))));
    }
    return cc_ok_CCResult_size_t_CCError(cc_value(wr));
}

#define cc_file_read_path(path, a) \
    (cc_file_read_path)((path), CC__ARENA_HANDLE(a))
#define cc_file_copy(src, dst, a) \
    (cc_file_copy)((src), (dst), CC__ARENA_HANDLE(a))
#define cc_script_print_file(path, a) \
    (cc_script_print_file)((path), CC__ARENA_HANDLE(a))

#endif /* CC_SCRIPT_FILE_H */
