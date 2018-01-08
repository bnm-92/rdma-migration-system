// util.tpp

#include <cstdlib>
#include <cstdio>
#include <errno.h>
#include <string.h>

inline
void die(const char* reason) {
    if (errno) {
        fprintf(stderr,
            "Error: %s failed (returned errno %d: %s).\n",
            reason, errno, strerror(errno));
    } else {
        fprintf(stderr,
            "Error: %s failed (returned zero or null).\n",
            reason);
    }
    exit(EXIT_FAILURE);

    printf("die called on: %s\n", reason);

}
