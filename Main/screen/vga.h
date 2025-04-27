#ifndef _VGA_H
#define _VGA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

extern uint32_t L8Clut[256];

void PrepareClut();
void LtdcInit();

#ifdef __cplusplus
}
#endif

#endif
