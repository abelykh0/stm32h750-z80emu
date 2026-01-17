#include "resources.h"
#include "stm32h7xx_hal.h"

uint8_t* ROM = (uint8_t*)QSPI_BASE;                       // opense.bin   16K
uint8_t* spectrumKeyboard = (uint8_t*)QSPI_BASE + 0x4000; // keyboard.bin 6.75K
uint8_t* font8x8 = (uint8_t*)QSPI_BASE + 0x6000;          // font8x8      2K
uint8_t* bmp1920x1080 = (uint8_t*)QSPI_BASE + 0x500000;   // 1920x1080 bmp file
