#ifndef VGA_VGACONFIG_H
#define VGA_VGACONFIG_H

#include <string.h>
#include <cstdint>
#include "stm32h7xx_hal.h"

// Board: WeAct STM32H750VBT6.
//
// The color pins below are wired (via the board's resistor DAC) to the
// same physical VGA connector lines this board's LTDC-based output uses,
// but LTDC isn't used here -- these are driven directly as GPIO, bit-banged
// through DMA, exactly like the STM32F407 version of this driver.
//
// GPIOB's high byte (bits 8-11) holds 4 of the resistor DAC's color bits
// (2 green, 2 blue) and nothing else -- GPIOB's low byte has QSPI flash
// pins on it, which is why red (the DAC's only GPIOB bit, PB0) is dropped:
// keeping strictly to the high byte means a single plain byte-wide ODR
// write can drive video every pixel without ever touching QSPI, VSYNC, SWD,
// or USB, exactly as simply as the F407 driver's single dedicated GPIO
// byte. 2 green bits x 2 blue bits = 16 colors, no red.
//
// PB10 Green bit 0 (was LTDC_G4)
// PB11 Green bit 1 (was LTDC_G5)
// PB8  Blue  bit 0 (was LTDC_B6)
// PB9  Blue  bit 1 (was LTDC_B7)
// PC6  HSync (TIM3_CH1)
// PA4  VSync (plain GPIO output)
#define VIDEO_GPIO_PORT GPIOB
#define VIDEO_GPIO_MASK 0x0F00u
#define VIDEO_GPIO_ODR_BYTE ((uint32_t)((uint8_t *)&GPIOB->ODR + 1))

#define SYNC_GPIO_PORT GPIOA
#define HSYNC_GPIO_PORT GPIOC
#define HSYNC_PIN 6u
#define VSYNC_PIN 4u

#define SYNC_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()
#define HSYNC_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define VIDEO_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()

// Packs green/blue intensities into the byte transferred to VIDEO_GPIO_ODR_BYTE:
// bits[1:0] = blue (PB8/PB9), bits[3:2] = green (PB10/PB11), bits[7:4] unused.
constexpr std::uint8_t vga_make_pixel(unsigned green /* 0-3 */, unsigned blue /* 0-3 */) {
  return static_cast<std::uint8_t>((blue & 0x3u) | ((green & 0x3u) << 2));
}

#endif  // VGA_VGACONFIG_H
