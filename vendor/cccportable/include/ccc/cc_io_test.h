/*
 * Test header for Result return sugar in .cch files.
 * Prefer T !>(E) + cc_ok/cc_err; CCRes(T, E) remains a macro alias.
 */
#ifndef CC_IO_TEST_H
#define CC_IO_TEST_H

#include <ccc/cc_io_error.h>
#include <ccc/cc_result.h>

static inline CCResult_bool_CCIoError cc_io_test_func(void) {
    return cc_ok_CCResult_bool_CCIoError(true);
}

static inline CCResult_int_CCError cc_io_test_int(int x) {
    if (x < 0) {
        return cc_err_CCResult_int_CCError(CC_ERROR(CC_ERR_INVALID_ARG, "negative"));
    }
    return cc_ok_CCResult_int_CCError(x * 2);
}

#endif /* CC_IO_TEST_H */
