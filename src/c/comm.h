#pragma once
#include <pebble.h>

// Initialize AppMessage and request weather from PKJS.
void comm_init(void);
void comm_deinit(void);

// Background mode: minimal init for wakeup-triggered fetches.
// Called when app launches with APP_LAUNCH_WAKEUP.
void comm_background_init(void);

// Load cached weather data before first draw (prevents units flash).
void comm_load_cache(void);

// Seconds since a unix timestamp, clamped at 0 when it is in the future.
// Use this for EVERY elapsed-time test against WeatherData.last_updated: that
// field carries the phone's clock while time(NULL) is the watch's, and plain
// unsigned subtraction turns a one-second lead into 4.29 billion. See comm.c.
uint32_t comm_secs_since(uint32_t then);

// Ask PKJS to refresh now (e.g. on theme/units change or manual refresh).
void comm_request_refresh(void);

// Phase 12: Ask PKJS to fetch a fresh radar composite from the proxy
// and stream the pixel buffer back as RadarChunk* messages.
void comm_request_radar(void);

// Called whenever weather_data is updated from inbox (so UI can redraw).
typedef void (*CommUpdateCb)(void);
void comm_set_update_callback(CommUpdateCb cb);

// Called after Clay changes per-card visibility or visual order
// (PhoneManagesCards on), so the app can re-apply the on-watch enable flags to
// nav and rebuild the traversal. Distinct from the redraw callback because it
// needs the settings→nav mapping the UI layer owns.
typedef void (*CommVisibilityCb)(void);
void comm_set_visibility_callback(CommVisibilityCb cb);

// Watch→Clay seed: push the current card order + per-card enable flags (+ the
// PhoneManagesCards opt-in) to PKJS, which caches them so the Clay config page
// always opens showing the watch's true state. Debounced ~1s so a chain of
// reorder long-presses coalesces into one message; retries a few times if the
// outbox is busy (e.g. the launch refresh request is still in flight).
// Best-effort: a lost push self-heals on the next change or launch.
void comm_send_card_state(void);
