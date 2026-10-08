/* Linux (PC addition): the v3-deps-158 linux prebuilts (chipmunk, ...) were built with
 * -ffast-math against glibc < 2.31, which still exported the __<fn>_finite entry points. Newer
 * glibc dropped them; forward them to the regular functions so the old archives link. */

#include <math.h>

float __powf_finite(float x, float y) { return powf(x, y); }
float __expf_finite(float x) { return expf(x); }
float __logf_finite(float x) { return logf(x); }
float __acosf_finite(float x) { return acosf(x); }
float __asinf_finite(float x) { return asinf(x); }
float __atan2f_finite(float y, float x) { return atan2f(y, x); }
float __sqrtf_finite(float x) { return sqrtf(x); }
double __pow_finite(double x, double y) { return pow(x, y); }
double __exp_finite(double x) { return exp(x); }
double __log_finite(double x) { return log(x); }
double __acos_finite(double x) { return acos(x); }
double __asin_finite(double x) { return asin(x); }
double __atan2_finite(double y, double x) { return atan2(y, x); }
double __sqrt_finite(double x) { return sqrt(x); }
