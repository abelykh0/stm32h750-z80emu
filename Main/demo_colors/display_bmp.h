#ifndef _DISPLAY_BMP_H_
#define _DISPLAY_BMP_H_

#include <stdint.h>
#include "fatfs.h"

#ifdef __cplusplus
extern "C" {
#endif

int load_bmp_image(FIL* bmp_file, uint8_t* indexed_image, uint32_t* clut);

#ifdef __cplusplus
}
#endif

#endif


