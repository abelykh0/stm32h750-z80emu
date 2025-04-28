#ifndef __STARTUP_H__
#define __STARTUP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"

void initialize();
void setup();
void loop();
bool onHardFault();

#ifdef __cplusplus
}
#endif

#endif /* __STARTUP_H__ */
