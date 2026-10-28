#ifndef TIF_TEST_CHECK_H
#define TIF_TEST_CHECK_H

#include <stdio.h>

static int check_failures = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,       \
                    __LINE__, #cond);                                    \
            check_failures++;                                            \
        }                                                                \
    } while (0)

#define CHECK_DONE() (check_failures == 0 ? 0 : 1)

#endif
