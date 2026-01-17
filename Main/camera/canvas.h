#ifndef __MCU_H__
#define __MCU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "usbd_video_conf.h"

#define CAMERA_TEXT_COLUMNS (UVC_WIDTH / 8)
#define CAMERA_TEXT_ROWS    (UVC_HEIGHT / 8)

// Chroma subsampling mode: 444, 422
#define CHROMA_FORMAT 444

#if (CHROMA_FORMAT == 444)
    #define H_SAMP_FACTOR 1
#elif (CHROMA_FORMAT == 422)
    #define H_SAMP_FACTOR 2
#else
    #error "Unsupported CHROMA_FORMAT"
#endif

#define MCU_WIDTH  (UVC_WIDTH / 8 / H_SAMP_FACTOR)
#define MCU_HEIGHT (UVC_HEIGHT / 8)

typedef struct
{
    uint8_t y[H_SAMP_FACTOR][8][8];
    uint8_t cb[8][8];
    uint8_t cr[8][8];
} __attribute__((packed)) Mcu;

extern uint8_t canvas[MCU_WIDTH * MCU_HEIGHT * sizeof(Mcu)];

void InitCamera();
void Clear(uint8_t color);
void SetPixel(uint16_t x, uint16_t y, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif
