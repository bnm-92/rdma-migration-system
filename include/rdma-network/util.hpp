#ifndef __UTIL_HPP__
#define __UTIL_HPP__

void die(const char* reason);

#define ASSERT_ZERO(x) \
    do { if ( (x)) die(#x); } while (0)
#define ASSERT_NONZERO(x) \
    do { if (!(x)) die(#x); } while (0)

#endif // __UTIL_HPP__
