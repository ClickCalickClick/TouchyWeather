#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../comm.h"
#include "../settings.h"
#include "../weather_data.h"
#include "../anim.h"
#include <pebble.h>
#include <string.h>
#include <stdlib.h>

// Phase 12: Radar card.
//
// Streaming pipeline:
//   1. On first card appearance the draw fn calls comm_request_radar()
//      which kicks PKJS to fetch the latest composite from the Vercel
//      proxy.
//   2. PKJS receives ~25,600 bytes of pre-quantized Pebble 8-bit pixels
//      (160x160) and ships them in 1500-byte chunks via AppMessage.
//   3. comm.c routes RadarChunk* tuples to card_radar_receive_chunk
//      which assembles a buffer and, on the final chunk, builds a
//      GBitmapFormat8Bit GBitmap and switches the card to READY.
//   4. card_radar_draw paints the bitmap centered with a small "+"
//      crosshair on the user's exact location and a "RAINVIEWER" footer.
//
// State machine:
//   IDLE     – never requested. Auto-requests on first appearance.
//   LOADING  – request in-flight. Shows "FETCHING RADAR…" + progress %.
//   READY    – bitmap drawn. Re-fetch only on manual refresh / 5min stale.
//   ERROR    – PKJS reported failure. Shows "RADAR UNAVAILABLE".
//
// All draw X coords prefixed by bounds.origin.x so the card slides
// correctly during nav transitions (Phase 11 lesson).

#define RADAR_CHUNK_SIZE 1500

typedef enum {
  RADAR_IDLE = 0,
  RADAR_LOADING,
  RADAR_READY,
  RADAR_ERROR,
} RadarState;

static RadarState s_state = RADAR_IDLE;
static uint8_t   *s_buf = NULL;
static int        s_buf_w = 0;
static int        s_buf_h = 0;
static int        s_buf_total = 0;
static int        s_chunks_total = 0;
static int        s_chunks_received = 0;
static GBitmap   *s_bitmap = NULL;
static uint32_t   s_frame_ts = 0;
static uint32_t   s_last_request_secs = 0;

static void prv_free_buf(void) {
  if (s_buf) { free(s_buf); s_buf = NULL; }
  s_buf_total = 0;
  s_chunks_total = 0;
  s_chunks_received = 0;
}

static void prv_free_bitmap(void) {
  if (s_bitmap) { gbitmap_destroy(s_bitmap); s_bitmap = NULL; }
}

static void prv_request_if_needed(void) {
  uint32_t now = (uint32_t)time(NULL);
  // Avoid hammering the proxy: 240s cooldown between auto-requests.
  // Aligned just under the proxy's 300s Cache-Control TTL so a normal
  // user navigating in/out of the radar card never re-hits the upstream
  // (and never counts against the Vercel WAF rate limit window).
  // Force-refresh (SELECT button) bypasses this — see
  // card_radar_force_refresh below.
  if (s_last_request_secs && (now - s_last_request_secs) < 240) return;
  s_last_request_secs = now;
  s_state = RADAR_LOADING;
  s_chunks_received = 0;
  comm_request_radar();
}

// Public entry point for the SELECT-button retry. Bypasses the 60s
// cooldown so the user can immediately re-request after a timeout or
// when the cached image is stale. Ignored while a fetch is in-flight.
void card_radar_force_refresh(void) {
  if (s_state == RADAR_LOADING) return; // already in-flight, ignore
  prv_free_bitmap();
  prv_free_buf();
  s_last_request_secs = 0; // clear cooldown
  s_state = RADAR_LOADING;
  s_chunks_received = 0;
  comm_request_radar();
}

void card_radar_receive_status(int status) {
  // 1 = loading (informational), 2 = ready (sent only after the proxy
  // responded successfully), 3 = error.
  if (status == 3) {
    s_state = RADAR_ERROR;
    prv_free_buf();
  }
}

void card_radar_receive_chunk(int chunk_idx, int chunk_total,
                              const uint8_t *data, int data_len,
                              int width, int height, uint32_t frame_ts) {
  if (chunk_idx == 0) {
    prv_free_buf();
    prv_free_bitmap();
    s_buf_w = width;
    s_buf_h = height;
    s_buf_total = width * height;
    s_chunks_total = chunk_total;
    s_chunks_received = 0;
    s_buf = (uint8_t *)malloc(s_buf_total);
    if (!s_buf) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "radar: malloc %d failed", s_buf_total);
      s_state = RADAR_ERROR;
      return;
    }
    memset(s_buf, 0, s_buf_total);
    if (frame_ts) s_frame_ts = frame_ts;
    s_state = RADAR_LOADING;
  }

  if (!s_buf) return;

  int offset = chunk_idx * RADAR_CHUNK_SIZE;
  int copy_len = data_len;
  if (offset < 0 || offset >= s_buf_total) return;
  if (offset + copy_len > s_buf_total) copy_len = s_buf_total - offset;
  if (copy_len > 0) memcpy(s_buf + offset, data, copy_len);
  s_chunks_received++;

  if (s_chunks_received >= s_chunks_total) {
    // Final chunk: build the GBitmap. gbitmap_create_blank may pad each
    // row to a 4-byte boundary; copy row by row to honor that.
    s_bitmap = gbitmap_create_blank(GSize(s_buf_w, s_buf_h),
                                    GBitmapFormat8Bit);
    if (s_bitmap) {
      uint8_t *bm = gbitmap_get_data(s_bitmap);
      int row_bytes = gbitmap_get_bytes_per_row(s_bitmap);
      for (int y = 0; y < s_buf_h; ++y) {
        memcpy(bm + y * row_bytes, s_buf + y * s_buf_w, s_buf_w);
      }
      s_state = RADAR_READY;
    } else {
      APP_LOG(APP_LOG_LEVEL_ERROR, "radar: gbitmap_create_blank failed");
      s_state = RADAR_ERROR;
    }
    // Free the staging buffer — bitmap has its own copy now.
    free(s_buf);
    s_buf = NULL;
  }
}

static void prv_draw_centered_text(GContext *ctx, GRect bounds, const char *s,
                                   GFont font, GColor color, int y) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, s, font,
      GRect(bounds.origin.x, y, bounds.size.w, 24),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void card_radar_draw(GContext *ctx, GRect bounds) {
  int W = bounds.size.w;
  int H = bounds.size.h;
  int ox = bounds.origin.x;

  // Header.
  ui_draw_card_header_with_icon(ctx, bounds, "RADAR",
                                theme_fg(),
                                UI_HEADER_Y, 18,
                                icon_draw_droplet);

  // Auto-request on first IDLE draw. Bounds.origin.x will be 0 once the
  // slide transition has settled — request anyway, the chunks will
  // arrive while we're displaying the card.
  if (s_state == RADAR_IDLE) {
    prv_request_if_needed();
  }

  // --- Big Mode (Stage B): bigger state text + crosshair, and ADD the
  // mandatory status banner (this is the one card that never drew it). Gated on
  // Big Mode so the Normal radar look is unchanged (Big-OFF pixel-identical).
  // Radar only navigates on emery/gabbro (large classes), so no small-class
  // tuning is needed. ---
  bool big = settings_get_big_mode();
  GFont body = big ? ui_font_body() : ui_font_header();
  GFont small = ui_font_label();

  if (s_state == RADAR_READY && s_bitmap) {
    // Center the bitmap in the card area below header / above footer.
    int img_w = s_buf_w;
    int img_h = s_buf_h;
    int img_x = ox + (W - img_w) / 2;
    int img_y = UI_HEADER_Y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(6, 4);
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, s_bitmap,
                                 GRect(img_x, img_y, img_w, img_h));

    // Crosshair on user location (image center). Bigger + thicker in Big Mode
    // so "you are here" is findable.
    int cx = img_x + img_w / 2;
    int cy = img_y + img_h / 2;
    int arm = big ? 8 : 4;
    graphics_context_set_stroke_color(ctx, theme_accent_orange());
    graphics_context_set_stroke_width(ctx, big ? 3 : 2);
    graphics_draw_line(ctx, GPoint(cx - arm, cy), GPoint(cx + arm, cy));
    graphics_draw_line(ctx, GPoint(cx, cy - arm), GPoint(cx, cy + arm));

    // Footer attribution required by RainViewer TOS. Anchored relative to
    // card bottom so it clears the card indicator on both rect and round.
    // Round needs more clearance due to the larger bottom bezel. In Big Mode
    // it lifts further to sit just above the added status banner.
    GFont tiny = ui_font_caption();
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
    // The constants below were the last private model of where the pill sits
    // (#75) — week.c's and advice.c's went in Phase 1, this one was missed
    // because the card is carved out under 128KB and so never draws here to
    // give the bug away. It is wrong: on SMALL_RECT `H - 35` = 133 puts the
    // attribution 7px BELOW the pill's own top edge, i.e. underneath it.
    // Derive it instead, so a card that is currently dark cannot come back
    // wrong if radar is ever enabled on a small unit. 16px is the caption's
    // line box; ui_content_bottom() already carries the 4px of air.
    int foot_y = ui_content_bottom(bounds) - 16;
#else
    int foot_y = big ? (H - PBL_IF_ROUND_ELSE(81, 66))
                     : (H - PBL_IF_ROUND_ELSE(51, 35));
#endif
    prv_draw_centered_text(ctx, bounds, "RAINVIEWER",
                           tiny, theme_secondary(), foot_y);
  } else if (s_state == RADAR_LOADING) {
    int center_y = UI_HEADER_Y + UI_HEADER_HEIGHT + (H - UI_HEADER_HEIGHT) / 2 - 20;
    prv_draw_centered_text(ctx, bounds, "FETCHING",
                           body, theme_fg(), center_y);
    prv_draw_centered_text(ctx, bounds, "RADAR\u2026",
                           body, theme_fg(), center_y + 22);
    if (s_chunks_total > 0) {
      char pct[12];
      int p = (s_chunks_received * 100) / s_chunks_total;
      if (p > 100) p = 100;
      snprintf(pct, sizeof(pct), "%d%%", p);
      prv_draw_centered_text(ctx, bounds, pct,
                             small, theme_secondary(), center_y + 50);
    }
  } else if (s_state == RADAR_ERROR) {
    int center_y = UI_HEADER_Y + UI_HEADER_HEIGHT + (H - UI_HEADER_HEIGHT) / 2 - 20;
    prv_draw_centered_text(ctx, bounds, "RADAR",
                           body, theme_fg(), center_y);
    prv_draw_centered_text(ctx, bounds, "UNAVAILABLE",
                           body, theme_fg(), center_y + 22);
    prv_draw_centered_text(ctx, bounds, "Check phone link",
                           small, theme_secondary(), center_y + 50);
  } else {
    int center_y = UI_HEADER_Y + UI_HEADER_HEIGHT + (H - UI_HEADER_HEIGHT) / 2 - 12;
    prv_draw_centered_text(ctx, bounds, "Loading\u2026",
                           body, theme_secondary(), center_y);
  }

  // Mandatory status banner in Big Mode \u2014 radar is the only card that never
  // drew one, so add it here (in every radar state) so the last-updated / rain
  // alert is always visible. Normal mode is unchanged (no banner) so the current
  // radar look is preserved.
  if (big) {
    WeatherData *d = weather_data_get();
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
  }
}
