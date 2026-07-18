#include "theme.h"
#include "settings.h"

#define PERSIST_KEY_THEME 1
// Retired: settings.c used to shadow-write the theme under key 200
// (settings_save_theme). theme.c is now the single source of truth; this
// key is only read once, for migration, then deleted so the two can't drift.
#define PERSIST_KEY_THEME_LEGACY 200

static ThemeMode s_mode = THEME_LIGHT;
static Window *s_window = NULL;

void theme_init(void) {
  if (persist_exists(PERSIST_KEY_THEME)) {
    s_mode = (ThemeMode)persist_read_int(PERSIST_KEY_THEME);
  } else if (persist_exists(PERSIST_KEY_THEME_LEGACY)) {
    // Only fall back to the legacy key when the canonical key is absent;
    // key 1 is always the freshest (theme_set writes it on every change).
    s_mode = persist_read_int(PERSIST_KEY_THEME_LEGACY) ? THEME_DARK : THEME_LIGHT;
    persist_write_int(PERSIST_KEY_THEME, (int)s_mode);
  } else {
    s_mode = THEME_LIGHT;
  }
  // Drop the retired shadow key so the two sources can never drift again.
  if (persist_exists(PERSIST_KEY_THEME_LEGACY)) {
    persist_delete(PERSIST_KEY_THEME_LEGACY);
  }
}

void theme_set(ThemeMode mode) {
  s_mode = mode;
  persist_write_int(PERSIST_KEY_THEME, (int)mode);
  // Auto-apply to bound window so callers don't have to remember.
  if (s_window) window_set_background_color(s_window, theme_bg());
}

ThemeMode theme_get(void) { return s_mode; }

GColor theme_bg(void) {
  return s_mode == THEME_DARK ? GColorBlack : GColorWhite;
}

GColor theme_fg(void) {
  return s_mode == THEME_DARK ? GColorWhite : GColorBlack;
}

GColor theme_muted(void) {
  return s_mode == THEME_DARK ? GColorDarkGray : GColorLightGray;
}

// Secondary body-text color: darker on light bg, lighter on dark bg.
// `theme_muted` looks fine on tracks/dividers but its light-mode value
// (LightGray on white) is unreadable for actual text. Use this for any
// body text that needs to be de-emphasized but still legible.
GColor theme_secondary(void) {
  // Big Mode (Stage B): high-contrast policy — de-emphasized body text is a
  // low-vision liability, so collapse it to full-contrast fg. (theme_muted is
  // deliberately NOT collapsed: it drives the inactive page-indicator dots,
  // which must stay distinct from the fg active dot.)
  if (settings_get_big_mode()) return theme_fg();
  return s_mode == THEME_DARK ? GColorLightGray : GColorDarkGray;
}

// --- Accent colors (Phase 5: B&W fallback; Stage B: Big-Mode high contrast) ---
// On 1-bit (aplite/diorite/flint) the SDK auto-reduces these accents to
// GColorWhite, which is INVISIBLE on the light theme's white background
// (the hi/lo temps, arrows, gauges and chart lines vanished on diorite).
// Fall the accents back to theme_fg() there instead — always the opposite
// of the background, so every accented element stays visible. Color
// differentiation is lost on B&W (all accents read as fg); elements that
// relied on hue for meaning are already differentiated by shape (up/down
// arrows, distinct icons, chart markers). Color platforms are unchanged.
//
// Big Mode (Stage B) applies the SAME collapse-to-fg on COLOR platforms: the
// high-contrast policy prioritizes strong figure/ground and colour-independent
// legibility over hue, and the Big-Mode card layouts already carry meaning by
// shape (↑/↓ arrows, distinct icons) — the same reason the B&W fallback is
// safe. A card that genuinely needs a hue to BE the data (e.g. the AQI category
// colour) can opt back out locally.
GColor theme_accent_orange(void) {
  // ~#FFAA00 — Pebble's chrome yellow / orange
  if (settings_get_big_mode()) return theme_fg();
  return PBL_IF_COLOR_ELSE(GColorChromeYellow, theme_fg());
}

GColor theme_accent_blue(void) {
  // ~#00AAFF — bright cyan/blue
  if (settings_get_big_mode()) return theme_fg();
  return PBL_IF_COLOR_ELSE(GColorVividCerulean, theme_fg());
}

GColor theme_accent_advice(void) {
  // Purple (GColorVividViolet) — distinct from orange (sunrise/UV/banner)
  // and blue (sunset/precip/AQ). Readable on both dark and light backgrounds.
  if (settings_get_big_mode()) return theme_fg();
  return PBL_IF_COLOR_ELSE(GColorVividViolet, theme_fg());
}

GColor theme_indicator_active(void) { return theme_fg(); }
GColor theme_indicator_inactive(void) { return theme_muted(); }

void theme_apply_to_window(Window *window) {
  s_window = window;
  if (window) window_set_background_color(window, theme_bg());
}
