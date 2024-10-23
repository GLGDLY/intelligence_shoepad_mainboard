#ifndef _MACRO_UTILS_H
#define _MACRO_UTILS_H

#include "soc/soc.h"

#define X_EXPAND_CNT(...) +1

#define MS_TO_FREQ(ms)	  (1000 / ms)
#define MS_TO_DIVIDER(ms) (APB_CLK_FREQ / MS_TO_FREQ(ms))

#endif // _MACRO_UTILS_H