#pragma once
#include <pebble.h>

void card_main_draw(GContext *ctx, GRect bounds);
void card_advice_draw(GContext *ctx, GRect bounds);
void card_hours_draw(GContext *ctx, GRect bounds);
void card_week_draw(GContext *ctx, GRect bounds);
void card_precipitation_draw(GContext *ctx, GRect bounds);
void card_uv_draw(GContext *ctx, GRect bounds);
void card_air_quality_draw(GContext *ctx, GRect bounds);
void card_sun_cycle_draw(GContext *ctx, GRect bounds);
void card_night_sky_draw(GContext *ctx, GRect bounds);
void card_golden_hour_draw(GContext *ctx, GRect bounds);
void card_radar_draw(GContext *ctx, GRect bounds);
void card_settings_draw(GContext *ctx, GRect bounds);

// Big Mode paging for the 6 Hours card. Big Mode draws 3 large rows instead
// of 6 dense ones, sampling every other hour; the two pages partition the same
// +1h..+6h window (page 0 = +1/+3/+5, page 1 = +2/+4/+6) so every hour the app
// holds is reachable. Returns false outside Big Mode, where the card already
// draws all six rows and the caller should leave SELECT/tap alone.
bool card_hours_pages_active(void);
void card_hours_page_next(void);

// Phase 12: Radar card chunk receiver. Called from comm.c when a radar
// pixel chunk arrives over AppMessage. On chunk_idx == 0 the card
// allocates a width*height byte buffer; subsequent chunks memcpy at
// offset chunk_idx * RADAR_CHUNK_SIZE; on the final chunk a GBitmap is
// created and the card transitions to READY state.
void card_radar_receive_chunk(int chunk_idx, int chunk_total,
                              const uint8_t *data, int data_len,
                              int width, int height, uint32_t frame_ts);
void card_radar_receive_status(int status);
// Force a fresh radar fetch bypassing the 60s cooldown. Called when
// the user presses SELECT on the Radar card.
void card_radar_force_refresh(void);
