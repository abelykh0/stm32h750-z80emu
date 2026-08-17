#include "vga.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "vgaConfig.h"
#include "copy_words.h"
#include "rasterizer.h"
#include "timing.h"

// Buffers the DMA reads from (must be DMA-capable) go in RAM_D2.
#define IN_SCAN_RAM  __attribute__((section(".vga_scan_ram")))
// The CPU-only rasterization target buffer goes in DTCM (fastest, but not
// DMA-reachable -- exactly why the two buffers are split like this).
#define IN_LOCAL_RAM __attribute__((section(".vga_local_ram")))

// Timing-critical ISR code goes in ITCM for zero-wait-state execution. Unlike
// the F407 version of this driver, nothing copies this section into place
// automatically -- init() does it manually (see below), since this project's
// linker script/startup file don't already have an ITCM copy mechanism.
#define RAM_CODE __attribute__((section(".vga_ramcode")))

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace vga {

/*******************************************************************************
 * Driver configuration.
 */

static constexpr unsigned
  // Used to adjust size of scan_buffer.
  max_pixels_per_line = 2200,
  // Fudge factor: shifts timer-initiated DRQ back in time by this many cycles,
  // to delay DRQ until DMA has started.
  drq_shift_cycles = 2,
  // Fudge factor: how long the shock absorber IRQ should lead the actual start
  // of video IRQ, in cycles.
  shock_absorber_shift_cycles = 20,
  // Amount of pad to place on either side of the working buffer, so that lazy
  // rasterizers can scribble slightly outside the lines -- in words.
  extra_pad_words = 4;


/*******************************************************************************
 * Driver state.
 */

// A copy of the current Timing, held in RAM for fast access.
static Timing current_timing;

// [0, current_mode.video_end_line).  Updated at front porch interrupt.
static unsigned volatile current_line;

/*
 * The vertical timing state.  This is a Gray code and the bits have meaning.
 * See the inspector functions below.
 */
enum class State {
  blank     = 0b00,
  starting  = 0b01,
  active    = 0b11,
  finishing = 0b10,
};

// Should we be producing a video signal?
inline bool is_displayed_state(State s) {
  return static_cast<unsigned>(s) & 0b10;
}

// Should we be rendering a scanline?
inline bool is_rendered_state(State s) {
  return static_cast<unsigned>(s) & 0b01;
}

// Finally, the actual variable.
static State volatile state;

// This is the DMA source for scan-out, copied from the working buffer during
// pend_sv.  It must be located in DMA-capable RAM (RAM_D2 here -- DTCM is
// *not* reachable by DMA on H7, mirroring how CCMRAM wasn't on F4).
//
// It contains an extra word's worth of pixels to ensure that we can follow
// every line with an extra transfer to blank the outputs.  The extra pixels
// are blanked after the rasterizer returns.
alignas(std::uint32_t) IN_SCAN_RAM
static Pixel scan_buffer[max_pixels_per_line + sizeof(std::uint32_t)];

// This is the working buffer, the target of the Rasterizer.  Its contents will
// be copied to the scan_buffer during hblank if needed.  It's in DTCM, the
// fastest RAM available to the CPU (and not reachable by DMA, which is fine --
// only the CPU ever touches this buffer).
//
// It has invisible padding at either end because it makes certain tile
// scrolling algorithms simpler to implement if they need not color precisely
// within the lines.
alignas(std::uint32_t) IN_LOCAL_RAM
static struct {
  std::uint32_t left_pad[extra_pad_words];
  Pixel buffer[max_pixels_per_line];
  std::uint32_t right_pad[extra_pad_words];
} working;

// A description of the contents of the working buffer, produced by the last
// Rasterizer that was applied.  This is used to adjust the output timings.
static Rasterizer::RasterInfo working_buffer_shape;

// When a Rasterizer completes and updates the working buffer, we set this
// flag.  This triggers a copy into the scan buffer at next hblank, at which
// time the flag is cleared.
static bool scan_buffer_needs_update;

// A pre-built DMA_SxCR value to be used to start the next DMA transfer.
// This is set up during hblank based on the working_buffer_shape, and consumed
// at start of active video.
static std::uint32_t next_dma_xfer_cr;
IN_LOCAL_RAM
static bool next_use_timer;

// The head of the linked list of Rasterizer bands.
static Band const *band_list_head;

// A copy of the band we're currently processing.
static Band current_band;

// A semaphore used to indicate, to the application, when the driver has
// begun processing the most recently configured band list.
static std::atomic<bool> band_list_taken{false};


/*******************************************************************************
 * ITCM setup.
 */

extern "C" std::uint32_t _sitcm, _eitcm, _siitcm;

static void copy_ramcode_to_itcm() {
  std::memcpy(&_sitcm, &_siitcm,
              reinterpret_cast<std::uintptr_t>(&_eitcm)
                - reinterpret_cast<std::uintptr_t>(&_sitcm));
}


/*******************************************************************************
 * Driver API.
 */

void init() {
  copy_ramcode_to_itcm();

  // Turn a bunch of stuff on.
  SYNC_GPIO_CLK_ENABLE();
  HSYNC_GPIO_CLK_ENABLE();
  VIDEO_GPIO_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  // Configure FIFO: quarter threshold, FIFO enabled (direct mode disabled),
  // FIFO error interrupt off.
  DMA2_Stream5->FCR = DMA_SxFCR_DMDIS;

  // Route DMA2 stream 5's request through DMAMUX1 (H7 has no fixed
  // channel-select field in DMA_SxCR like F4 -- routing is external).
  // DMA2 streams 0-7 map to DMAMUX1 channels 8-15.
  DMAMUX1_Channel13->CCR = DMA_REQUEST_TIM1_UP;

  // Configure the pixel-generation timer used during reduced-horizontal mode.
  // TIM1's kernel clock is 240MHz in this board's fixed clock config (same as
  // TIM2/TIM3's, conveniently -- see timing.h).  We'll load ARR under
  // rasterizer control to synthesize 1/n rates.
  __HAL_RCC_TIM1_CLK_ENABLE();
  TIM1->PSC = 0;  // Divide input clock by 1.
  TIM1->CR1 = TIM_CR1_URS;
  TIM1->DIER = TIM_DIER_UDE;  // DRQ on update

  // Configure our interrupt priorities.  The scheme is:
  //  TIM3 (horizontal) gets highest priority.
  //  TIM2 (shock absorber) is set just lower.
  //  PendSV (rendering, user code) is lowest.
  NVIC_SetPriority(TIM3_IRQn, 0);
  NVIC_SetPriority(TIM2_IRQn, 1);
  NVIC_SetPriority(PendSV_IRQn, (1 << __NVIC_PRIO_BITS) - 1);

  // Halt all our timers on debug.
  DBGMCU->APB1LFZ1 |= DBGMCU_APB1LFZ1_DBG_TIM3 | DBGMCU_APB1LFZ1_DBG_TIM2;
  DBGMCU->APB2FZ1 |= DBGMCU_APB2FZ1_DBG_TIM1;

  band_list_head = nullptr;
  band_list_taken = false;

  sync_off();
  video_off();
}

void sync_off() {
  GPIO_InitTypeDef gpio = {};
  gpio.Pin = 1u << VSYNC_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(SYNC_GPIO_PORT, &gpio);

  gpio.Pin = 1u << HSYNC_PIN;
  HAL_GPIO_Init(HSYNC_GPIO_PORT, &gpio);
}

void video_off() {
  GPIO_InitTypeDef gpio = {};
  gpio.Pin = VIDEO_GPIO_MASK;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(VIDEO_GPIO_PORT, &gpio);
}

void sync_on() {
  // Configure the hsync pin to produce hsync via TIM3_CH1 (AF2).
  GPIO_InitTypeDef gpio = {};
  gpio.Pin = 1u << HSYNC_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(HSYNC_GPIO_PORT, &gpio);

  // Configure the vsync pin as a plain GPIO output.
  gpio.Pin = 1u << VSYNC_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = 0;
  HAL_GPIO_Init(SYNC_GPIO_PORT, &gpio);
}

void video_on() {
  GPIO_InitTypeDef gpio = {};
  gpio.Pin = VIDEO_GPIO_MASK;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(VIDEO_GPIO_PORT, &gpio);
}

/*
 * Sets up one of the two horizontal timers (TIM3, the physical HSYNC/CH1
 * output, and TIM2, its "shock absorber" trigger source), which share almost
 * all of their init code.
 */
static void configure_h_timer(Timing const &timing, TIM_TypeDef *tim) {
  tim->PSC = timing.cycles_per_pixel - 1;
  tim->ARR = timing.line_pixels - 1;

  bool negative = timing.hsync_polarity == Timing::Polarity::negative;

  // CH1 (PC6 on TIM3; unrouted to any pin on TIM2, which is harmless).
  tim->CCR1 = timing.sync_pixels;
  tim->CCMR1 = (tim->CCMR1 & ~(TIM_CCMR1_CC1S | TIM_CCMR1_OC1M))
             | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1;  // PWM mode 1
  tim->CCER = (tim->CCER & ~TIM_CCER_CC1P)
            | TIM_CCER_CC1E
            | (negative ? TIM_CCER_CC1P : 0);

  tim->CCR2 = timing.sync_pixels
            + timing.back_porch_pixels - timing.video_lead;
  tim->CCR3 = timing.sync_pixels
            + timing.back_porch_pixels + timing.video_pixels;
}

void configure_timing(Timing const &timing) {
  // Disable outputs during mode change.
  sync_off();
  video_off();

  // Place the horizontal timers in reset, disabling interrupts.
  NVIC_DisableIRQ(TIM3_IRQn);
  __HAL_RCC_TIM3_FORCE_RESET();
  NVIC_ClearPendingIRQ(TIM3_IRQn);

  NVIC_DisableIRQ(TIM2_IRQn);
  __HAL_RCC_TIM2_FORCE_RESET();
  NVIC_ClearPendingIRQ(TIM2_IRQn);

  // Busy-wait for pending DMA to complete.
  while (DMA2_Stream5->CR & DMA_SxCR_EN) {}

  // No scanout strategy can achieve fewer than 4 cycles per pixel.
  assert(timing.cycles_per_pixel >= 4);
  assert(timing.line_pixels <= max_pixels_per_line);

  // Bring TIM2/TIM3 back out of reset.
  __HAL_RCC_TIM3_RELEASE_RESET();
  __HAL_RCC_TIM2_RELEASE_RESET();
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_TIM2_CLK_ENABLE();

  // Configure TIM2/3 for horizontal sync generation.
  configure_h_timer(timing, TIM2);
  configure_h_timer(timing, TIM3);

  // Adjust tim2's CC2 value back in time.
  TIM2->CCR2 = TIM2->CCR2 - shock_absorber_shift_cycles;

  // Configure tim2 to distribute its enable signal as its trigger output.
  TIM2->CR2 = (TIM2->CR2 & ~(TIM_CR2_MMS | TIM_CR2_CCDS)) | TIM_CR2_MMS_0;

  // Configure tim3 to trigger from tim2 and run forever.
  // NOTE: assumes TIM3's ITR1 input is wired to TIM2's TRGO, matching the
  // classic F1/F4/F7/H7 TIM2-5 internal trigger table. Verify this against
  // RM0433's "TIMx internal trigger connection" table (or CubeMX's timer
  // inspector) before relying on it -- if wrong, TIM3 simply never starts
  // (no video), it won't misbehave in a more surprising way.
  TIM3->SMCR = (TIM3->SMCR & ~(TIM_SMCR_TS | TIM_SMCR_SMS))
             | TIM_SMCR_TS_0                       // ITR1 (TIM2)
             | TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1;    // Trigger mode

  // Turn on tim3's interrupts.
  TIM3->DIER = TIM_DIER_CC2IE     // Interrupt at start of active video.
             | TIM_DIER_CC3IE;    // Interrupt at end of active video.

  // Turn on only one of tim2's
  TIM2->DIER = TIM_DIER_CC2IE;    // Interrupt at start of active video.

  // Note: timers still not running.

  switch (timing.vsync_polarity) {
    case Timing::Polarity::positive:
      HAL_GPIO_WritePin(SYNC_GPIO_PORT, 1u << VSYNC_PIN, GPIO_PIN_RESET);
      break;
    case Timing::Polarity::negative:
      HAL_GPIO_WritePin(SYNC_GPIO_PORT, 1u << VSYNC_PIN, GPIO_PIN_SET);
      break;
  }

  // Scribble over working buffer to help catch bugs.
  for (std::size_t i = 0; i < sizeof(working.buffer); i += 2) {
    working.buffer[i] = 0xFF;
    working.buffer[i + 1] = 0x00;
  }

  // Blank the final word of the scan buffer.
  for (unsigned i = 0; i < sizeof(std::uint32_t); ++i) {
    scan_buffer[timing.video_pixels + i] = 0;
  }

  // Set up global state.
  current_line = 0;
  current_timing = timing;
  state = State::blank;
  working_buffer_shape = {
    .offset = 0,
    .length = 0,
    .cycles_per_pixel = timing.cycles_per_pixel,
    .repeat_lines = 0,
  };
  next_use_timer = false;

  scan_buffer_needs_update = false;

  // Start TIM2, which starts TIM3.
  NVIC_EnableIRQ(TIM2_IRQn);
  NVIC_EnableIRQ(TIM3_IRQn);
  TIM2->CR1 |= TIM_CR1_CEN;

  sync_on();
}

void configure_band_list(Band const *head) {
  band_list_head = head;
  band_list_taken = false;
}

void clear_band_list() {
  configure_band_list(nullptr);
  while (!band_list_taken) __WFI();
}

void wait_for_vblank() {
  while (!in_vblank()) __WFI();
}

bool in_vblank() {
  return current_line < current_timing.video_start_line;
}

void sync_to_vblank() {
  while (in_vblank()) __WFI();
  wait_for_vblank();
}

/*******************************************************************************
 * Horizontal timing implementation.  See also the ISR, declared outside of
 * namespace vga toward the end of the file.
 */

RAM_CODE
static void start_of_active_video() {
  // The start-of-active-video (SAV) event is only significant during visible
  // lines.
  if (UNLIKELY(!is_displayed_state(state))) return;

  // Clear stream 5 flags (HIFCR is a write-1-to-clear register).
  DMA2->HIFCR = DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CTEIF5
              | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTCIF5;

  // Start the countdown for first DRQ.
  TIM1->CR1 = TIM_CR1_URS | (next_use_timer ? TIM_CR1_CEN : 0);

  DMA2_Stream5->CR = next_dma_xfer_cr;
}

RAM_CODE
static void end_of_active_video() {
  // The end-of-active-video (EAV) event is always significant, as it advances
  // the line state machine and kicks off PendSV.

  // Shut off TIM1; only really matters in reduced-horizontal mode.
  TIM1->CR1 = TIM_CR1_URS;

  // Apply timing changes requested by the last rasterizer.
  TIM3->CCR2 = current_timing.sync_pixels
             + current_timing.back_porch_pixels - current_timing.video_lead
             + working_buffer_shape.offset;

  // Pend a PendSV to process hblank tasks.
  SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

  // We've finished this line; figure out what to do on the next one.
  unsigned next_line = current_line + 1;

  if (next_line == current_timing.vsync_start_line
      || next_line == current_timing.vsync_end_line) {
    // Either edge of vsync pulse.
    SYNC_GPIO_PORT->ODR ^= (1u << VSYNC_PIN);
  } else if (next_line == std::uint16_t(current_timing.video_start_line - 1)) {
    // We're one line before scanout begins -- need to start rasterizing.
    state = State::starting;
    if (band_list_head) {
      current_band = *band_list_head;
    } else {
      current_band = { nullptr, 0, nullptr };
    }
    band_list_taken = true;
  } else if (next_line == current_timing.video_start_line) {
    // Time to start output.  This will cause PendSV to copy rasterization
    // output into place for scanout, and the next SAV will start DMA.
    state = State::active;
  } else if (next_line == std::uint16_t(current_timing.video_end_line - 1)) {
    // For the final line, suppress rasterization but continue preparing
    // previously rasterized data for scanout, and continue starting DMA in
    // SAV.
    state = State::finishing;
  } else if (next_line == std::uint16_t(current_timing.video_end_line)) {
    // All done!  Suppress all scanout activity.
    state = State::blank;
    next_line = 0;
  }

  current_line = next_line;
}

void default_hblank_interrupt() __attribute__((weak));  // decl hack
RAM_CODE void default_hblank_interrupt() {}


/*******************************************************************************
 * Rasterization interface.  These are implementation factors of the PendSV
 * ISR.
 */

/*
 * Advances the current rasterizer band, possibly switching it for the next if
 * we've reached the end.  The 'edge' parameter is used only in the recursive
 * case.  (It would not appear at all if this language had nested functions.)
 */
RAM_CODE
static bool advance_rasterizer_band(bool edge = false) {
  if (current_band.line_count) {
    --current_band.line_count;
    return edge;
  }

  if (current_band.next) {
    current_band = *current_band.next;
    return advance_rasterizer_band(true);
  } else {
    current_band = { nullptr, 0, nullptr };
    return edge;
  }
}

/*
 * Transfers the contents of the working buffer into the scan buffer, if
 * necessary.
 */
RAM_CODE
static void update_scan_buffer() {
  if (scan_buffer_needs_update) {
    // Flip working_buffer into scan_buffer.  We know its contents are ready
    // because of the scan_buffer_needs_update flag.  Note that the flag may
    // not have been set, even in a displayed state, if we're repeating a
    // line.
    //
    // Note that GCC can't see that we've aligned the buffers correctly, so we
    // have to do a multi-cast dance. :-/
    copy_words(
        reinterpret_cast<std::uint32_t const *>(
          static_cast<void *>(working.buffer)),
        reinterpret_cast<std::uint32_t *>(
          static_cast<void *>(scan_buffer)),
        (working_buffer_shape.length + sizeof(std::uint32_t) - 1)
          / sizeof(std::uint32_t));
    for (unsigned i = 0; i < sizeof(std::uint32_t); ++i) {
      scan_buffer[working_buffer_shape.length + i] = 0;
    }
    scan_buffer_needs_update = false;
  }
}

/*
 * Prepares a configuration for the DMA stream and configures the horizontal
 * timer, if it's relevant to this mode.
 */
RAM_CODE
static void prepare_for_scanout() {
  DMA2_Stream5->CR &= ~DMA_SxCR_EN;

  if (working_buffer_shape.cycles_per_pixel > 4) {
    // Adjust reload frequency of TIM1 to accomodate desired pixel clock.
    // (ARR value is period - 1.)
    TIM1->ARR = working_buffer_shape.cycles_per_pixel - 1;
    // Force an update to reset the timer state.
    TIM1->EGR = TIM_EGR_UG;
    // Configure the timer as *almost* ready to produce a DRQ, less a small
    // value (fudge factor).  Gotta do this after the update event, above,
    // because that clears CNT.
    TIM1->CNT = TIM1->ARR - drq_shift_cycles;
    TIM1->SR = 0;

    DMA2_Stream5->PAR = VIDEO_GPIO_ODR_BYTE;
    DMA2_Stream5->M0AR = reinterpret_cast<std::uint32_t>(&scan_buffer);

    // The number of bytes read must exactly match the number of bytes written,
    // or the DMA controller will freak out.  Thus, we must adapt the transfer
    // size to the number of bytes transferred.
    std::uint32_t msize;
    switch (working_buffer_shape.length & 3) {
      case 0:
        msize = DMA_SxCR_MSIZE_1;  // word
        DMA2_Stream5->NDTR = working_buffer_shape.length + sizeof(std::uint32_t);
        break;

      case 2:
        msize = DMA_SxCR_MSIZE_0;  // half-word
        DMA2_Stream5->NDTR = working_buffer_shape.length + sizeof(std::uint16_t);
        break;

      default:
        msize = 0;  // byte
        DMA2_Stream5->NDTR = working_buffer_shape.length + sizeof(std::uint8_t);
        break;
    }

    next_dma_xfer_cr = DMA_SxCR_PL_1 | DMA_SxCR_PL_0
        | (1UL << DMA_SxCR_DIR_Pos)  // memory-to-peripheral
        | msize
        | DMA_SxCR_MINC             // psize = byte (0), pinc = false
        | DMA_SxCR_EN;
    next_use_timer = true;

  } else {
    // Note that we're using memory as the peripheral side.
    // This DMA controller is a little odd.
    DMA2_Stream5->PAR = reinterpret_cast<std::uint32_t>(&scan_buffer);
    DMA2_Stream5->M0AR = VIDEO_GPIO_ODR_BYTE;

    std::uint32_t psize;
    switch (working_buffer_shape.length & 3) {
      case 0:
        psize = DMA_SxCR_PSIZE_1;  // word
        DMA2_Stream5->NDTR = working_buffer_shape.length / sizeof(std::uint32_t) + 1;
        break;

      case 2:
        psize = DMA_SxCR_PSIZE_0;  // half-word
        DMA2_Stream5->NDTR = working_buffer_shape.length / sizeof(std::uint16_t) + 1;
        break;

      default:
        psize = 0;  // byte
        DMA2_Stream5->NDTR = working_buffer_shape.length / sizeof(std::uint8_t) + 1;
        break;
    }

    next_dma_xfer_cr = DMA_SxCR_PL_1 | DMA_SxCR_PL_0
        | (2UL << DMA_SxCR_DIR_Pos)  // memory-to-memory
        | psize
        | DMA_SxCR_PINC             // msize = byte (0), minc = false
        | DMA_SxCR_EN;
    next_use_timer = false;
  }
}

/*
 * Generates pixels for the *next* line, not the currently displaying one.
 */
RAM_CODE
static void rasterize_next_line() {
  auto const &timing = current_timing;
  auto next_line = current_line + 1;
  auto visible_line = next_line - timing.video_start_line;

  bool band_edge = advance_rasterizer_band();
  if (working_buffer_shape.repeat_lines == 0 || band_edge) {
    auto r = current_band.rasterizer;
    if (r) {
      working_buffer_shape = r->rasterize(current_timing.cycles_per_pixel,
                                          visible_line,
                                          working.buffer);
    } else {
      working_buffer_shape = {
        .offset = 0,
        .length = 0,
        .cycles_per_pixel = current_timing.cycles_per_pixel,
        .repeat_lines = 0,
      };
    }
    scan_buffer_needs_update = true;
  } else {  // repeat_lines > 0, not band_edge
    --working_buffer_shape.repeat_lines;
  }
}

}  // namespace vga


/*******************************************************************************
 * ISRs and user interrupt hook
 */

void vga_hblank_interrupt()
  __attribute__((weak, alias("_ZN3vga24default_hblank_interruptEv")));

extern "C" RAM_CODE void TIM2_IRQHandler() {
  // Shock absorber: fires slightly before TIM3's start-of-active-video
  // interrupt, purely to idle the processor so the pipeline/caches are
  // primed by the time the real, timing-critical interrupt arrives.
  TIM2->SR = static_cast<std::uint16_t>(~TIM_SR_CC2IF);
  __WFI();
}

extern "C" RAM_CODE void TIM3_IRQHandler() {
  auto sr = TIM3->SR;

  if (LIKELY(sr & TIM_SR_CC2IF)) {
    TIM3->SR = static_cast<std::uint16_t>(~TIM_SR_CC2IF);
    vga::start_of_active_video();
    return;
  }

  if (sr & TIM_SR_CC3IF) {
    TIM3->SR = static_cast<std::uint16_t>(~TIM_SR_CC3IF);
    vga::end_of_active_video();
    return;
  }
}

extern "C" RAM_CODE void PendSV_Handler() {
  if (LIKELY(is_displayed_state(vga::state))) {
    vga::update_scan_buffer();
    vga::prepare_for_scanout();
  }

  vga_hblank_interrupt();

  if (LIKELY(is_rendered_state(vga::state))) {
    vga::rasterize_next_line();
  }
}
