#include "timing.h"

#include "stm32h7xx_hal.h"

namespace vga {

// Common to every mode: 25MHz crystal / PLL1M=5 -> 5MHz VCO input (RANGE_2),
// PLL1P=2. D1CPRE=1, HPRE=2 (HCLK = sys_ck/2), so sys_ck = 2*HCLK. Only
// PLL1N (and hence sys_ck/HCLK) differs per mode.
//
// FLASH_LATENCY_4 is used uniformly -- it's what this board's default
// 480MHz config already uses, and more wait states than strictly necessary
// at a lower frequency is always safe, just not maximally fast.

// PLL1N=80 -> VCO=400MHz (medium range) -> sys_ck=200MHz -> HCLK=100MHz.
// HCLK <= 120MHz, so APB divisor is 1 (no APB timer-clock doubling needed
// to keep TIM1/2/3's kernel clock equal to HCLK).
// pixel clock = HCLK/4 = 25MHz (real VESA 640x480@60 clock is 25.175MHz,
// -0.7%). Standard 640x480@60 line/frame proportions otherwise unchanged,
// so hsync ~31.25kHz (vs 31.469kHz) and refresh ~59.5Hz (vs 60Hz) -- both
// within any multisync display's tolerance.
Timing const timing_640x480_60hz = {
  .clock_config = {
    .crystal_hz   = 25000000,
    .pll_m        = 5,
    .pll_n        = 80,
    .pll_p        = 2,  // H7 PLL1P is a plain even integer, not a macro like F4's.
    .pll_vcosel   = RCC_PLL1VCOMEDIUM,
    .pll_vcirange = RCC_PLL1VCIRANGE_2,
    .d1cpre       = RCC_SYSCLK_DIV1,
    .hpre         = RCC_HCLK_DIV2,
    .apb_divisor  = RCC_APB1_DIV1,
    .flash_latency = FLASH_LATENCY_4,
  },
  .cycles_per_pixel = 4,

  .line_pixels       = 800,
  .sync_pixels       = 96,
  .back_porch_pixels = 48,
  .video_lead        = 4,   // Fudge factor; may need retuning on real hardware.
  .video_pixels      = 640,
  .hsync_polarity    = Timing::Polarity::negative,

  .vsync_start_line = 10,
  .vsync_end_line   = 12,
  .video_start_line = 45,
  .video_end_line   = 525,
  .vsync_polarity   = Timing::Polarity::negative,
};

// PLL1N=128 -> VCO=640MHz (wide range) -> sys_ck=320MHz -> HCLK=160MHz.
// HCLK > 120MHz, so APB divisor is 2 (APB clock 80MHz, doubled by the timer
// clock rule back up to 160MHz = HCLK).
// pixel clock = HCLK/4 = 40MHz exactly -- the real VESA 800x600@60 clock.
// With the standard line/frame counts, this mode is timing-exact: hsync
// 37.879kHz and refresh 60.317Hz both match the standard precisely.
Timing const timing_800x600_60hz = {
  .clock_config = {
    .crystal_hz   = 25000000,
    .pll_m        = 5,
    .pll_n        = 128,
    .pll_p        = 2,  // H7 PLL1P is a plain even integer, not a macro like F4's.
    .pll_vcosel   = RCC_PLL1VCOWIDE,
    .pll_vcirange = RCC_PLL1VCIRANGE_2,
    .d1cpre       = RCC_SYSCLK_DIV1,
    .hpre         = RCC_HCLK_DIV2,
    .apb_divisor  = RCC_APB1_DIV2,
    .flash_latency = FLASH_LATENCY_4,
  },
  .cycles_per_pixel = 4,

  .line_pixels       = 1056,
  .sync_pixels       = 128,
  .back_porch_pixels = 88,
  .video_lead        = 4,   // Fudge factor; may need retuning on real hardware.
  .video_pixels      = 800,
  .hsync_polarity    = Timing::Polarity::positive,

  .vsync_start_line = 1,
  .vsync_end_line   = 5,
  .video_start_line = 28,
  .video_end_line   = 628,
  .vsync_polarity   = Timing::Polarity::positive,
};

// This mode needs ~229MHz HCLK, which would need a ~916MHz VCO with the
// d1cpre=1/hpre=2 pattern the two modes above use (exceeding the 836MHz wide
// range max) -- so this one uses hpre=1 instead: sys_ck = HCLK directly, no
// 2x CPU-clock headroom over HCLK like the others get, since we're already
// close to the ceiling for this mode.
// PLL1N=92 -> VCO=460MHz (wide range) -> sys_ck=230MHz -> HCLK=230MHz (hpre=1).
// HCLK > 120MHz, so APB divisor is 2, as above.
// pixel clock = HCLK/4 = 57.5MHz (real clock is 57.283MHz, +0.38%). This is
// the classic Mac 13"/14" RGB timing -- note it's actually 74.55Hz, not 60,
// despite the "75hz" in the name (matching how this mode is usually referred
// to informally). Standard proportions otherwise unchanged, so hsync
// ~49.9kHz (vs 49.72kHz) and refresh ~74.8Hz (vs 74.55Hz).
Timing const timing_832x624_75hz = {
  .clock_config = {
    .crystal_hz   = 25000000,
    .pll_m        = 5,
    .pll_n        = 92,
    .pll_p        = 2,  // H7 PLL1P is a plain even integer, not a macro like F4's.
    .pll_vcosel   = RCC_PLL1VCOWIDE,
    .pll_vcirange = RCC_PLL1VCIRANGE_2,
    .d1cpre       = RCC_SYSCLK_DIV1,
    .hpre         = RCC_HCLK_DIV1,
    .apb_divisor  = RCC_APB1_DIV2,
    .flash_latency = FLASH_LATENCY_4,
  },
  .cycles_per_pixel = 4,

  .line_pixels       = 1152,
  .sync_pixels       = 64,
  .back_porch_pixels = 224,
  .video_lead        = 4,   // Fudge factor; may need retuning on real hardware.
  .video_pixels      = 832,
  .hsync_polarity    = Timing::Polarity::negative,

  .vsync_start_line = 1,
  .vsync_end_line   = 4,
  .video_start_line = 43,
  .video_end_line   = 667,
  .vsync_polarity   = Timing::Polarity::negative,
};

}  // namespace vga
