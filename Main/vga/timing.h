#ifndef VGA_TIMING_H
#define VGA_TIMING_H

#include <cstdint>

namespace vga {

/*
 * Describes the PLL1/bus configuration needed to derive a given AHB (HCLK)
 * frequency from the board's 25 MHz crystal, mirroring the fields of HAL's
 * RCC_OscInitTypeDef/RCC_ClkInitTypeDef.
 *
 * H750's DMA-to-GPIO path (like every STM32 in this family) can only move
 * one byte per 4 AHB cycles at best -- that ratio is fixed in hardware, not
 * something firmware can adjust. So, like the original m4vgalib design (and
 * the STM32F407 version of this driver), the way to hit a *specific* pixel
 * clock is to reprogram the CPU/bus clock per mode so that HCLK/4 lands on
 * it, not to inflate cycles_per_pixel on a fixed clock.
 *
 * The catch specific to H7: HCLK is capped at 240 MHz regardless of how
 * high the Cortex-M7 core clock (up to 480 MHz) goes -- AHB1-4 are only
 * rated to 240 MHz. That puts a hard ceiling of 240/4 = 60 MHz on the
 * achievable pixel clock (vs. ~42 MHz on the F407's 168 MHz AHB), no matter
 * how the PLL is configured. Modes whose real pixel clock exceeds that
 * (1024x768, 1920x1080 below) can't be hit natively; see their comments in
 * timing.cc for how they're approximated instead.
 */
struct ClockConfig {
  std::uint32_t crystal_hz;       // External crystal frequency.
  std::uint32_t pll_m;            // PLL1M: divides crystal down to the VCO input range.
  std::uint32_t pll_n;            // PLL1N: multiplies up to the VCO frequency.
  std::uint32_t pll_p;            // PLL1P: divides VCO down to sys_ck (must be even).
  std::uint32_t pll_vcosel;       // RCC_PLL1VCOWIDE or RCC_PLL1VCOMEDIUM.
  std::uint32_t pll_vcirange;     // RCC_PLL1VCIRANGE_x, matching crystal_hz/pll_m.

  std::uint32_t d1cpre;           // RCC_SYSCLK_DIVx: sys_ck -> CPU core clock.
  std::uint32_t hpre;             // RCC_HCLK_DIVx: CPU core clock -> HCLK (AHB).
  std::uint32_t apb_divisor;      // RCC_APB1_DIVx == RCC_APB2_DIVx (kept equal
                                   // so TIM1/TIM2/TIM3's kernel clock, after
                                   // the "APB timer clock doubling" rule,
                                   // always ends up exactly equal to HCLK).

  std::uint32_t flash_latency;    // FLASH_LATENCY_x at this frequency.
};

/*
 * Describes the horizontal and vertical timing for a display mode, including
 * the outer bounds of active video.
 */
struct Timing {
  enum class Polarity {
    positive = 0,
    negative = 1,
  };

  /*
   * The pixel clock is derived from HCLK by a fixed multiplier (below),
   * making the CPU/bus clock configuration an integral part of the video
   * timing -- see ClockConfig's comment.
   */
  ClockConfig clock_config;

  /*
   * Number of HCLK cycles per pixel.  Every mode below uses 4 (the fastest,
   * most direct DMA path -- see vga.cc) except where the mode's real pixel
   * clock exceeds the 60 MHz ceiling described above, in which case HCLK is
   * simply maxed out at 240 MHz and the result is a slower-than-nominal but
   * otherwise ordinary 4-cycles-per-pixel mode.
   */
  std::uint16_t cycles_per_pixel;

  /*
   * Horizontal timing, expressed in pixels.
   *
   * The horizontal sync pulse implicitly starts at pixel zero of the line.
   *
   * Some of this information is redundant; it's stored this way to avoid
   * having to rederive it in the driver.
   */
  std::uint16_t line_pixels;        // Total, including blanking.
  std::uint16_t sync_pixels;        // Length of pulse.
  std::uint16_t back_porch_pixels;  // Between end of sync and start of video.
  std::uint16_t video_lead;         // Fudge factor: nudge DMA back in time.
  std::uint16_t video_pixels;       // Maximum pixels in active video.
  Polarity      hsync_polarity;     // Polarity of hsync pulse.

  /*
   * Vertical timing, expressed in lines.
   *
   * Because vertical timing is done in software, it's a little more flexible
   * than horizontal timing.
   */
  std::uint16_t vsync_start_line;  // Top edge of sync pulse.
  std::uint16_t vsync_end_line;    // Bottom edge of sync pulse.
  std::uint16_t video_start_line;  // Top edge of active video.
  std::uint16_t video_end_line;    // Bottom edge of active video.
  Polarity      vsync_polarity;    // Polarity of vsync pulse.
};

/*
 * Canned timings at the board's 25 MHz crystal frequency, each hitting its
 * real native pixel clock (or within a fraction of a percent of it) via
 * per-mode PLL1 reprogramming -- all comfortably under H750's 240 MHz HCLK
 * ceiling (60 MHz pixel clock max at 4 cycles/pixel). Modes whose real pixel
 * clock exceeds that ceiling (e.g. 1024x768's 65 MHz, 1920x1080's 148.5 MHz)
 * aren't included here.
 */
extern Timing const timing_640x480_60hz;
extern Timing const timing_800x600_60hz;
extern Timing const timing_832x624_75hz;  // Real rate is 74.55Hz, not 75.

}  // namespace vga

#endif  // VGA_TIMING_H
