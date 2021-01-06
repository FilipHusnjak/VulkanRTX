#pragma once

#ifdef NDEBUG
#define result_assert(expression)                                                                  \
    if (!(expression))                                                                             \
        *((char *)0) = 0;
#else
#define result_assert(expression) expression
#endif