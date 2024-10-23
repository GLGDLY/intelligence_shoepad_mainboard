#ifndef _MACRO_UTILS_H
#define _MACRO_UTILS_H

#include "soc/soc.h"

#define X_EXPAND_CNT(...) +1

#define MS_TO_FREQ(ms) (1000 / (ms))
#define MS_TO_US(ms)   ((ms)*1000)

#endif // _MACRO_UTILS_H