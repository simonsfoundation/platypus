#ifndef CRADLE_COMPAT_H
#define CRADLE_COMPAT_H

#include <math.h>

#if _MSC_VER == 1700
namespace std 
{
	static inline double log2(double v) { return log(v) / log(2.0); }
	static inline double exp2(double v) { return pow(2.0, v); }
	static inline int round(double v) { return int(v + 0.5); }
}
#endif

#endif