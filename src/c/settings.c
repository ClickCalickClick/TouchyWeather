#include "settings.h"
#include "theme.h"

// Persistence keys. Avoid collisions with comm.c PERSIST_KEY_CACHE=101.
// (Key 200 retired: theme is now owned solely by theme.c under persist key 1;
//  theme_init() migrates any legacy key-200 value. See theme.c.)
#define KEY_LOOP_NAV           201  // bool: wrap card carousel at edges
#define KEY_BG_UPDATE_INTERVAL 202  // int: background update interval in seconds
#define KEY_ANIMATIONS_ENABLED 203  // bool: decorative animation master switch
#define KEY_SELECT_TOGGLES_THEME 204 // bool: SELECT flips theme on ordinary cards
#define KEY_PHONE_MANAGES_CARDS 205  // bool: Clay controls per-card visibility
#define KEY_TOGGLE_BASE        210  // KEY_TOGGLE_BASE + ToggleId
#define KEY_CARD_ORDER         220  // SETTINGS_TOGGLEABLE_COUNT bytes

// Default: loop the carousel (wrap at the first/last card).
static bool s_loop_nav = true;

// Decorative animation master switch. Default on (preserves current behavior).
static bool s_animations_enabled = true;

// Whether SELECT toggles theme on ordinary cards. Default on (preserves the
// reflexive-press behavior users expect today).
static bool s_select_toggles_theme = true;

// Opt-in: phone (Clay) controls per-card visibility. Default off so on-watch
// card management is the norm and a Clay save can't silently wipe it.
static bool s_phone_manages_cards = false;

// Background update interval in seconds. Default is 0 (disabled, opt-in).
// 0 = disabled, 1800 = 30 mins, 3600 = 1 hour (recommended when enabled).
static int s_bg_update_interval = 0;

// Default visual order of rows in the Settings card. Decoupled from
// the enum order so Touch & Go appears second (after the locked MAIN
// row) without disrupting the persisted toggle keys
// (KEY_TOGGLE_BASE + ToggleId). User reordering mutates s_visual_order
// in place; the defaults are restored only when the persisted order
// is missing or invalid.
static const uint8_t s_default_order[SETTINGS_TOGGLEABLE_COUNT] = {
  TOGGLE_ADVICE,  // "TOUCH & GO" — second row, right after MAIN
  TOGGLE_HOURS,
  TOGGLE_WEEK,
  TOGGLE_PRECIP,
  TOGGLE_UV,
  TOGGLE_AQ,
  TOGGLE_SUN,
  TOGGLE_NIGHT,
  TOGGLE_GOLDEN,
  TOGGLE_RADAR,
};

static uint8_t s_visual_order[SETTINGS_TOGGLEABLE_COUNT];

static void prv_reset_order_to_default(void) {
  for (int i = 0; i < SETTINGS_TOGGLEABLE_COUNT; ++i) {
    s_visual_order[i] = s_default_order[i];
  }
}

static bool prv_is_valid_permutation(const uint8_t *buf) {
  bool seen[SETTINGS_TOGGLEABLE_COUNT] = {0};
  for (int i = 0; i < SETTINGS_TOGGLEABLE_COUNT; ++i) {
    uint8_t v = buf[i];
    if (v >= SETTINGS_TOGGLEABLE_COUNT) return false;
    if (seen[v]) return false;
    seen[v] = true;
  }
  return true;
}

static void prv_persist_order(void) {
  persist_write_data(KEY_CARD_ORDER, s_visual_order,
                     SETTINGS_TOGGLEABLE_COUNT);
}

ToggleId settings_visual_id(int visual_pos) {
  if (visual_pos < 0 || visual_pos >= SETTINGS_TOGGLEABLE_COUNT) return TOGGLE_HOURS;
  return (ToggleId)s_visual_order[visual_pos];
}

static bool s_enabled[SETTINGS_TOGGLEABLE_COUNT] = {
  true, true, true, true, true, true, true, true, true, true
};
static int s_cursor = 0;

static const char *s_labels[SETTINGS_TOGGLEABLE_COUNT] = {
  "6 HOURS", "WEEK AHEAD", "RAIN", "UV INDEX",
  "AIR QUAL", "SUN CYCLE", "NIGHT SKY", "GOLDEN HR", "RADAR",
  "TOUCH & GO",
};

void settings_load(void) {
  prv_reset_order_to_default();
  if (persist_exists(KEY_CARD_ORDER)) {
    uint8_t buf[SETTINGS_TOGGLEABLE_COUNT];
    int n = persist_read_data(KEY_CARD_ORDER, buf, sizeof(buf));
    if (n == SETTINGS_TOGGLEABLE_COUNT && prv_is_valid_permutation(buf)) {
      for (int i = 0; i < SETTINGS_TOGGLEABLE_COUNT; ++i) {
        s_visual_order[i] = buf[i];
      }
    }
  }
  // Theme is loaded by theme_init() (persist key 1), which runs before
  // settings_load(); no theme handling here anymore.
  for (int i = 0; i < SETTINGS_TOGGLEABLE_COUNT; ++i) {
    if (persist_exists(KEY_TOGGLE_BASE + i)) {
      s_enabled[i] = persist_read_bool(KEY_TOGGLE_BASE + i);
    }
  }
  if (persist_exists(KEY_LOOP_NAV)) {
    s_loop_nav = persist_read_bool(KEY_LOOP_NAV);
  }
  if (persist_exists(KEY_ANIMATIONS_ENABLED)) {
    s_animations_enabled = persist_read_bool(KEY_ANIMATIONS_ENABLED);
  }
  if (persist_exists(KEY_SELECT_TOGGLES_THEME)) {
    s_select_toggles_theme = persist_read_bool(KEY_SELECT_TOGGLES_THEME);
  }
  if (persist_exists(KEY_PHONE_MANAGES_CARDS)) {
    s_phone_manages_cards = persist_read_bool(KEY_PHONE_MANAGES_CARDS);
  }
  if (persist_exists(KEY_BG_UPDATE_INTERVAL)) {
    int iv = persist_read_int(KEY_BG_UPDATE_INTERVAL);
    // Sanity-guard the persisted interval. A prior build misparsed the
    // Clay string value and could persist garbage (e.g. ~808M from the
    // ASCII of "1800"), which scheduled a wakeup decades out. Accept only
    // 0 (disabled) or a sane range; otherwise fall back to disabled so
    // the corrupted value self-heals on the next launch.
    if (iv == 0 || (iv >= 300 && iv <= 86400)) {
      s_bg_update_interval = iv;
    } else {
      s_bg_update_interval = 0;
      persist_write_int(KEY_BG_UPDATE_INTERVAL, 0);
    }
  }
}

bool settings_get_loop_nav(void) {
  return s_loop_nav;
}

void settings_set_loop_nav(bool loop) {
  s_loop_nav = loop;
  persist_write_bool(KEY_LOOP_NAV, loop);
}

int settings_get_background_interval(void) {
  return s_bg_update_interval;
}

void settings_set_background_interval(int interval_secs) {
  s_bg_update_interval = interval_secs;
  persist_write_int(KEY_BG_UPDATE_INTERVAL, interval_secs);
}

bool settings_get_animations_enabled(void) {
  return s_animations_enabled;
}

void settings_set_animations_enabled(bool enabled) {
  s_animations_enabled = enabled;
  persist_write_bool(KEY_ANIMATIONS_ENABLED, enabled);
}

bool settings_get_select_toggles_theme(void) {
  return s_select_toggles_theme;
}

void settings_set_select_toggles_theme(bool enabled) {
  s_select_toggles_theme = enabled;
  persist_write_bool(KEY_SELECT_TOGGLES_THEME, enabled);
}

bool settings_get_phone_manages_cards(void) {
  return s_phone_manages_cards;
}

void settings_set_phone_manages_cards(bool enabled) {
  s_phone_manages_cards = enabled;
  persist_write_bool(KEY_PHONE_MANAGES_CARDS, enabled);
}

bool settings_get_enabled(ToggleId id) {
  if (id >= SETTINGS_TOGGLEABLE_COUNT) return true;
  return s_enabled[id];
}

void settings_set_enabled(ToggleId id, bool enabled) {
  if (id >= SETTINGS_TOGGLEABLE_COUNT) return;
  s_enabled[id] = enabled;
  persist_write_bool(KEY_TOGGLE_BASE + id, enabled);
}

const char *settings_label(ToggleId id) {
  if (id >= SETTINGS_TOGGLEABLE_COUNT) return "";
  return s_labels[id];
}

int settings_cursor(void) { return s_cursor; }

void settings_cursor_advance(void) {
  s_cursor = (s_cursor + 1) % SETTINGS_TOGGLEABLE_COUNT;
}

bool settings_move_up(int visual_pos) {
  if (visual_pos <= 0 || visual_pos >= SETTINGS_TOGGLEABLE_COUNT) return false;
  uint8_t tmp = s_visual_order[visual_pos - 1];
  s_visual_order[visual_pos - 1] = s_visual_order[visual_pos];
  s_visual_order[visual_pos] = tmp;
  s_cursor = visual_pos - 1;
  prv_persist_order();
  return true;
}

bool settings_move_down(int visual_pos) {
  if (visual_pos < 0 || visual_pos >= SETTINGS_TOGGLEABLE_COUNT - 1) return false;
  uint8_t tmp = s_visual_order[visual_pos + 1];
  s_visual_order[visual_pos + 1] = s_visual_order[visual_pos];
  s_visual_order[visual_pos] = tmp;
  s_cursor = visual_pos + 1;
  prv_persist_order();
  return true;
}
