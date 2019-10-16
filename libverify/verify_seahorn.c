/**
 * Defines assert and require implementations for static analysis.
 * TODO(scottwe): sol_assert/sol_require may require more complicated behaviour
 *                later on... it would make sense for each to call into a "ll"
 *                implementation.
 * @date 2019
 */

#include "verify.h"

#include "seahorn/seahorn.h"

// -------------------------------------------------------------------------- //

void sol_setup(int _argc, const char **_argv) {}

// -------------------------------------------------------------------------- //

void sol_on_transaction(void) {}

// -------------------------------------------------------------------------- //

void ll_assume(sol_raw_uint8_t _cond)
{
    assume(_cond);
}

// -------------------------------------------------------------------------- //

void sol_assert(sol_raw_uint8_t _cond, const char* _msg)
{
    sassert(_cond);
}

void sol_require(sol_raw_uint8_t _cond, const char* _msg)
{
    (void) _msg;
    ll_assume(_cond);
}

// -------------------------------------------------------------------------- //

/**
 * NOTE: nd calls are intentionally unimplemented such that they are truly non-
 *       deterministic, unimplemented functions.
 */

// -------------------------------------------------------------------------- //