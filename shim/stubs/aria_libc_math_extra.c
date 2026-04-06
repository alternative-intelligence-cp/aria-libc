// Extra math shim functions
#include <math.h>
#include <stdint.h>
#include <string.h>

int aria_libc_math_isnan(double x) { return isnan(x); }
int aria_libc_math_isinf(double x) { return isinf(x); }
int aria_libc_math_isfinite(double x) { return isfinite(x); }
double aria_libc_math_copysign(double x, double y) { return copysign(x, y); }
double aria_libc_math_fma(double x, double y, double z) { return fma(x, y, z); }
double aria_libc_math_hypot(double x, double y) { return hypot(x, y); }

// Bit-level float interpretation
int64_t aria_libc_math_flt64_bits(double x) {
    int64_t r; memcpy(&r, &x, 8); return r;
}
double aria_libc_math_flt64_from_bits(int64_t bits) {
    double r; memcpy(&r, &bits, 8); return r;
}
int32_t aria_libc_math_flt32_bits(double x) {
    float f = (float)x;
    int32_t r; memcpy(&r, &f, 4); return r;
}
double aria_libc_math_flt32_from_bits(int32_t bits) {
    float f; memcpy(&f, &bits, 4); return (double)f;
}
