#include "timing.h"

namespace vga {

// cycles_per_pixel = 10 -> 240MHz/10 = 24MHz pixel clock (standard is
// 25.175MHz, -4.7%). Same VESA line/frame structure as standard 640x480@60,
// so the whole timing is uniformly ~4.7% slow: hsync ~30kHz (vs 31.469kHz),
// refresh ~57.1Hz (vs 60Hz). Well within what a multisync display tolerates.
Timing const timing_640x480_60hz = {
  .cycles_per_pixel = 10,

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

// cycles_per_pixel = 6 -> 240MHz/6 = 40MHz exactly, which is the real
// standard 800x600@60Hz pixel clock. This mode is timing-exact.
Timing const timing_800x600_60hz = {
  .cycles_per_pixel = 6,

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

// cycles_per_pixel = 4 is this driver's fastest possible setting (a 60MHz
// pixel clock ceiling), still short of the real 1024x768@60 clock (65MHz).
// Using the standard VESA line/frame counts as-is at 60MHz gives hsync
// ~44.6kHz (vs 48.4kHz) and refresh ~55.4Hz (vs 60Hz) -- about 7.7% slow,
// but the driver can still address the full 1024 nominal samples per line
// (whether a given Rasterizer can actually produce that many bytes within
// one line's worth of CPU time is a separate question -- see rasterizer.h).
Timing const timing_1024x768_60hz = {
  .cycles_per_pixel = 4,

  .line_pixels       = 1344,
  .sync_pixels       = 136,
  .back_porch_pixels = 160,
  .video_lead        = 4,   // Fudge factor; may need retuning on real hardware.
  .video_pixels      = 1024,
  .hsync_polarity    = Timing::Polarity::negative,

  .vsync_start_line = 3,
  .vsync_end_line   = 9,
  .video_start_line = 38,
  .video_end_line   = 806,
  .vsync_polarity   = Timing::Polarity::negative,
};

// The real 1920x1080@60 pixel clock (148.5MHz) is far beyond what this
// fixed-240MHz-clock, bit-banged driver can produce (60MHz ceiling), so
// unlike the modes above this doesn't use the standard pixel counts scaled
// to 60MHz (that would give a wildly wrong ~27kHz hsync that no display
// would accept as "1080p"). Instead, line_pixels is chosen so hsync/vsync
// land almost exactly on the real CEA-861 1920x1080p60 frequencies
// (67.5kHz / 60Hz) -- vertical line counts are unchanged from the standard,
// since those don't depend on pixel clock at all -- while video_pixels
// (776) is the *nominal* max addressable samples per line at this driver's
// 60MHz ceiling, far short of 1920. In practice a Rasterizer for this mode
// will render far fewer distinct samples than even that (see RasterInfo::
// cycles_per_pixel), stretched to fill the line: real vertical resolution
// stays a genuine 1080 lines, but horizontal detail will be noticeably
// coarse. Good chance of a real monitor syncing to this as "1080p", though.
Timing const timing_1920x1080_60hz = {
  .cycles_per_pixel = 4,

  .line_pixels       = 889,
  .sync_pixels       = 18,
  .back_porch_pixels = 60,
  .video_lead        = 4,   // Fudge factor; may need retuning on real hardware.
  .video_pixels      = 776,
  .hsync_polarity    = Timing::Polarity::positive,

  .vsync_start_line = 4,
  .vsync_end_line   = 9,
  .video_start_line = 45,
  .video_end_line   = 1125,
  .vsync_polarity   = Timing::Polarity::positive,
};

}  // namespace vga
