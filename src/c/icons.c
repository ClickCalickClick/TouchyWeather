#include "icons.h"
#include "theme.h"

// Condition-icon palette. On 1-bit displays (diorite/flint) the hardcoded
// accents quantize by luminance: ChromeYellow and VividCerulean both reduce
// to white, so the sun/clear glyph and the rain drops disappear entirely on
// the light theme, and the icon appears to move between themes. Remap to
// theme-aware fg/muted there so every condition stays visible in both
// themes. Color platforms keep the original constants.
//
// Fog is the one condition drawn from strokes alone, and a stroke does not
// dither: theme_muted() quantizes to the *background* in both themes
// (LightGray->white on light, DarkGray->black on dark), so sharing
// ICON_CLOUD_COLOR erased the glyph entirely. theme_secondary() is the
// inverse pair, so it quantizes to the foreground in both.
#if defined(PBL_BW)
#define ICON_SUN_COLOR   (theme_fg())
#define ICON_CLOUD_COLOR (theme_muted())
#define ICON_DROP_COLOR  (theme_fg())
#define ICON_BOLT_COLOR  (theme_fg())
#define ICON_STORM_CLOUD (theme_muted())
#define ICON_FOG_COLOR   (theme_secondary())
#else
#define ICON_SUN_COLOR   GColorChromeYellow
#define ICON_CLOUD_COLOR GColorLightGray
#define ICON_DROP_COLOR  GColorVividCerulean
#define ICON_BOLT_COLOR  GColorChromeYellow
#define ICON_STORM_CLOUD GColorDarkGray
#define ICON_FOG_COLOR   GColorLightGray
#endif

static void set_stroke(GContext *ctx, GColor color, int w) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, w);
  graphics_context_set_antialiased(ctx, true);
}

void icon_draw_sun(GContext *ctx, GPoint c, int size, GColor color) {
  int r_disc = size / 4;
  int r_ray_in = size / 4 + 2;
  int r_ray_out = size / 2;
  set_stroke(ctx, color, 3);
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, c, r_disc);
  // 8 rays
  for (int i = 0; i < 8; ++i) {
    int32_t a = TRIG_MAX_ANGLE * i / 8;
    GPoint p1 = {
      c.x + (int16_t)(sin_lookup(a) * r_ray_in / TRIG_MAX_RATIO),
      c.y - (int16_t)(cos_lookup(a) * r_ray_in / TRIG_MAX_RATIO)
    };
    GPoint p2 = {
      c.x + (int16_t)(sin_lookup(a) * r_ray_out / TRIG_MAX_RATIO),
      c.y - (int16_t)(cos_lookup(a) * r_ray_out / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, p1, p2);
  }
}

void icon_draw_cloud(GContext *ctx, GPoint c, int size, GColor color) {
  // A cloud built from 3 overlapping circles + bottom flat strip.
  int r1 = size * 5 / 16;       // big middle bump
  int r2 = size * 3 / 16;       // smaller side bumps
  int width = size;
  int height = size * 5 / 8;
  GPoint left  = { c.x - width/4, c.y + 2 };
  GPoint right = { c.x + width/4, c.y + 2 };
  GPoint top   = { c.x,           c.y - height/4 };
  graphics_context_set_fill_color(ctx, color);
  set_stroke(ctx, color, 2);
  graphics_fill_circle(ctx, top, r1);
  graphics_fill_circle(ctx, left, r2);
  graphics_fill_circle(ctx, right, r2);
  GRect base = GRect(c.x - width/2 + 2, c.y, width - 4, height/3);
  graphics_fill_rect(ctx, base, 4, GCornersBottom);
}

void icon_draw_cloud_rain(GContext *ctx, GPoint c, int size,
                          GColor cloud_color, GColor drop_color) {
  GPoint cloud_c = { c.x, c.y - size/8 };
  icon_draw_cloud(ctx, cloud_c, size * 7 / 8, cloud_color);
  // 3 drops
  set_stroke(ctx, drop_color, 2);
  int drop_top_y = c.y + size/4;
  int drop_bot_y = c.y + size/2;
  int spacing = size / 4;
  for (int i = -1; i <= 1; ++i) {
    GPoint p1 = { c.x + i * spacing, drop_top_y };
    GPoint p2 = { c.x + i * spacing - 2, drop_bot_y };
    graphics_draw_line(ctx, p1, p2);
  }
}

void icon_draw_snow(GContext *ctx, GPoint c, int size, GColor color) {
  set_stroke(ctx, color, 2);
  int r = size / 2 - 2;
  // 3 lines through center at 0°, 60°, 120°.
  for (int i = 0; i < 3; ++i) {
    int32_t a = TRIG_MAX_ANGLE * i / 6;
    GPoint p1 = {
      c.x + (int16_t)(sin_lookup(a) * r / TRIG_MAX_RATIO),
      c.y - (int16_t)(cos_lookup(a) * r / TRIG_MAX_RATIO)
    };
    GPoint p2 = {
      c.x - (int16_t)(sin_lookup(a) * r / TRIG_MAX_RATIO),
      c.y + (int16_t)(cos_lookup(a) * r / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, p1, p2);
  }
}

void icon_draw_lightning(GContext *ctx, GPoint c, int size, GColor color) {
  GPathInfo info = (GPathInfo) {
    .num_points = 6,
    .points = (GPoint[]) {
      {  2, -size/2 },
      { -size/4,  0 },
      {  0,  0 },
      { -2,  size/2 },
      {  size/4,  0 },
      {  0,  0 },
    }
  };
  GPath *p = gpath_create(&info);
  gpath_move_to(p, c);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, p);
  gpath_destroy(p);
}

void icon_draw_fog(GContext *ctx, GPoint c, int size, GColor color) {
  set_stroke(ctx, color, 3);
  int half = size / 2;
  int gap = size / 5;
  graphics_draw_line(ctx, GPoint(c.x - half + 2, c.y - gap),
                          GPoint(c.x + half - 2, c.y - gap));
  graphics_draw_line(ctx, GPoint(c.x - half + 4, c.y),
                          GPoint(c.x + half - 4, c.y));
  graphics_draw_line(ctx, GPoint(c.x - half + 2, c.y + gap),
                          GPoint(c.x + half - 2, c.y + gap));
}

void icon_draw_condition(GContext *ctx, GPoint c, int size,
                         WeatherCondition cond) {
  switch (cond) {
    case COND_SUNNY:
      icon_draw_sun(ctx, c, size, ICON_SUN_COLOR); break;
    case COND_PARTLY_CLOUDY: {
      icon_draw_sun(ctx, GPoint(c.x - size/4, c.y - size/4), size*3/4, ICON_SUN_COLOR);
      icon_draw_cloud(ctx, GPoint(c.x + size/6, c.y + size/8), size*3/4, ICON_CLOUD_COLOR);
      break;
    }
    case COND_CLOUDY:
      icon_draw_cloud(ctx, c, size, ICON_CLOUD_COLOR); break;
    case COND_RAIN:
      icon_draw_cloud_rain(ctx, c, size, ICON_CLOUD_COLOR, ICON_DROP_COLOR); break;
    case COND_SNOW:
      icon_draw_cloud(ctx, GPoint(c.x, c.y - size/6), size*3/4, ICON_CLOUD_COLOR);
      icon_draw_snow(ctx, GPoint(c.x, c.y + size/4), size/2, ICON_DROP_COLOR);
      break;
    case COND_STORM:
      icon_draw_cloud(ctx, GPoint(c.x, c.y - size/6), size*7/8, ICON_STORM_CLOUD);
      icon_draw_lightning(ctx, GPoint(c.x, c.y + size/4), size/2, ICON_BOLT_COLOR);
      break;
    case COND_FOG:
      icon_draw_fog(ctx, c, size, ICON_FOG_COLOR); break;
  }
}

void icon_draw_droplet(GContext *ctx, GPoint c, int size, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  // Teardrop: triangle on top, circle on bottom.
  int r = size * 3 / 8;
  GPoint cb = { c.x, c.y + size/8 };
  graphics_fill_circle(ctx, cb, r);
  GPathInfo info = (GPathInfo) {
    .num_points = 3,
    .points = (GPoint[]) {
      { 0,    -size/2 },
      { -r+1,  size/8 },
      {  r-1,  size/8 },
    }
  };
  GPath *p = gpath_create(&info);
  gpath_move_to(p, c);
  gpath_draw_filled(ctx, p);
  gpath_destroy(p);
}

void icon_draw_wind(GContext *ctx, GPoint c, int size, GColor color) {
  set_stroke(ctx, color, 3);
  int w = size;
  // Three horizontal lines with hooked ends, like Pebble's wind glyph.
  int y1 = c.y - size/4;
  int y2 = c.y;
  int y3 = c.y + size/4;
  graphics_draw_line(ctx, GPoint(c.x - w/2, y1), GPoint(c.x + w/2 - 4, y1));
  // hook
  graphics_draw_line(ctx, GPoint(c.x + w/2 - 4, y1), GPoint(c.x + w/2, y1 - 3));
  graphics_draw_line(ctx, GPoint(c.x - w/2, y2), GPoint(c.x + w/2 - 8, y2));
  graphics_draw_line(ctx, GPoint(c.x + w/2 - 8, y2), GPoint(c.x + w/2 - 4, y2 - 3));
  graphics_draw_line(ctx, GPoint(c.x - w/2, y3), GPoint(c.x + w/2 - 6, y3));
}

void icon_draw_arrow_up(GContext *ctx, GPoint c, int size, GColor color) {
  set_stroke(ctx, color, 3);
  // Vertical shaft.
  graphics_draw_line(ctx, GPoint(c.x, c.y - size/2), GPoint(c.x, c.y + size/2));
  // Chevron arms point down-left and down-right from the tip.
  GPoint tip = { c.x, c.y - size/2 };
  int arm = size/2;
  graphics_draw_line(ctx, tip, GPoint(tip.x - arm, tip.y + arm));
  graphics_draw_line(ctx, tip, GPoint(tip.x + arm, tip.y + arm));
}

void icon_draw_arrow_down(GContext *ctx, GPoint c, int size, GColor color) {
  set_stroke(ctx, color, 3);
  graphics_draw_line(ctx, GPoint(c.x, c.y - size/2), GPoint(c.x, c.y + size/2));
  GPoint tip = { c.x, c.y + size/2 };
  int arm = size/2;
  graphics_draw_line(ctx, tip, GPoint(tip.x - arm, tip.y - arm));
  graphics_draw_line(ctx, tip, GPoint(tip.x + arm, tip.y - arm));
}

// Maps a compass abbreviation to a 16-point index (0=N, clockwise).
// Returns -1 for unrecognized strings.
static int compass_to_index(const char *dir) {
  static const char *names[16] = {
    "N","NNE","NE","ENE","E","ESE","SE","SSE",
    "S","SSW","SW","WSW","W","WNW","NW","NNW"
  };
  if (!dir) return -1;
  for (int i = 0; i < 16; ++i) {
    if (strcmp(dir, names[i]) == 0) return i;
  }
  return -1;
}

void icon_draw_compass_arrow(GContext *ctx, GPoint c, int size,
                             GColor color, const char *dir) {
  int idx = compass_to_index(dir);
  if (idx < 0) idx = 0;  // default: point north
  int32_t angle = TRIG_MAX_ANGLE * idx / 16;

  // Filled arrowhead defined pointing up (north), centered on origin.
  int h = size / 2;
  int w = size * 3 / 8;
  GPathInfo info = (GPathInfo) {
    .num_points = 4,
    .points = (GPoint[]) {
      { 0,  -h },      // tip
      { -w,  h },      // bottom-left
      { 0,   h - 2 }, // notch
      {  w,  h },      // bottom-right
    }
  };
  GPath *p = gpath_create(&info);
  gpath_rotate_to(p, angle);
  gpath_move_to(p, c);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, p);
  gpath_destroy(p);
}

static void draw_half_sun_with_rays(GContext *ctx, GPoint c, int size, GColor color) {
  // c is the center of the horizon line. Half-disc sits above it.
  int r = size * 3 / 10;
  GRect disc = GRect(c.x - r, c.y - r, r * 2, r * 2);
  graphics_context_set_fill_color(ctx, color);
  // Top half-disc (180° arc from -90 to +90 in Pebble's coordinate where 0 = up).
  graphics_fill_radial(ctx, disc, GOvalScaleModeFitCircle, r,
                       DEG_TO_TRIGANGLE(-90), DEG_TO_TRIGANGLE(90));
  set_stroke(ctx, color, 2);
  // Horizon line.
  graphics_draw_line(ctx, GPoint(c.x - size/2, c.y),
                          GPoint(c.x + size/2, c.y));
  // 3 outward rays from sun.
  int ray_inner = r + 2;
  int ray_outer = r + 6;
  // Top ray (straight up).
  graphics_draw_line(ctx, GPoint(c.x, c.y - ray_inner),
                          GPoint(c.x, c.y - ray_outer));
  // Upper-left and upper-right diagonal rays at ~45°.
  int d_in = (ray_inner * 7) / 10;
  int d_out = (ray_outer * 7) / 10;
  graphics_draw_line(ctx, GPoint(c.x - d_in, c.y - d_in),
                          GPoint(c.x - d_out, c.y - d_out));
  graphics_draw_line(ctx, GPoint(c.x + d_in, c.y - d_in),
                          GPoint(c.x + d_out, c.y - d_out));
}

void icon_draw_sunrise(GContext *ctx, GPoint c, int size, GColor color) {
  draw_half_sun_with_rays(ctx, c, size, color);
  // Up-arrow under the horizon, centered.
  set_stroke(ctx, color, 2);
  int ax = c.x;
  int top_y = c.y + 4;
  int bot_y = c.y + 4 + size/3;
  graphics_draw_line(ctx, GPoint(ax, top_y), GPoint(ax, bot_y));
  // chevron at top pointing up
  graphics_draw_line(ctx, GPoint(ax, top_y), GPoint(ax - 3, top_y + 3));
  graphics_draw_line(ctx, GPoint(ax, top_y), GPoint(ax + 3, top_y + 3));
}

void icon_draw_sunset(GContext *ctx, GPoint c, int size, GColor color) {
  draw_half_sun_with_rays(ctx, c, size, color);
  // Down-arrow under the horizon, centered.
  set_stroke(ctx, color, 2);
  int ax = c.x;
  int top_y = c.y + 4;
  int bot_y = c.y + 4 + size/3;
  graphics_draw_line(ctx, GPoint(ax, top_y), GPoint(ax, bot_y));
  // chevron at bottom pointing down
  graphics_draw_line(ctx, GPoint(ax, bot_y), GPoint(ax - 3, bot_y - 3));
  graphics_draw_line(ctx, GPoint(ax, bot_y), GPoint(ax + 3, bot_y - 3));
}

void icon_draw_pulse(GContext *ctx, GPoint c, int size, GColor color) {
  set_stroke(ctx, color, 2);
  int w = size;
  int x = c.x - w/2;
  GPoint pts[] = {
    { x,         c.y },
    { x + w/6,   c.y },
    { x + w/4,   c.y - size/3 },
    { x + w/3,   c.y + size/3 },
    { x + w/2,   c.y - size/4 },
    { x + 2*w/3, c.y },
    { x + w,     c.y },
  };
  for (size_t i = 0; i + 1 < sizeof(pts)/sizeof(pts[0]); ++i) {
    graphics_draw_line(ctx, pts[i], pts[i+1]);
  }
}

// Fingertip-tap glyph: a filled center dot with two concentric arc
// "ripple" rings opening upward and to the sides. Used as the
// identity icon for the Touch & Go advice card so it reads
// distinctly from the pulse icon used by Air Quality.
void icon_draw_tap(GContext *ctx, GPoint c, int size, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  // Filled center dot.
  int r_dot = size / 6;
  if (r_dot < 2) r_dot = 2;
  graphics_fill_circle(ctx, c, r_dot);
  // Two concentric arcs opening upward (-135 to +135 deg, leaving a
  // small "shadow" gap at the bottom so it reads as a tap, not a target).
  int r_inner = size / 3;
  int r_outer = size / 2;
  set_stroke(ctx, color, 2);
  GRect bb_inner = GRect(c.x - r_inner, c.y - r_inner, r_inner * 2, r_inner * 2);
  GRect bb_outer = GRect(c.x - r_outer, c.y - r_outer, r_outer * 2, r_outer * 2);
  graphics_draw_arc(ctx, bb_inner, GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(-135), DEG_TO_TRIGANGLE(135));
  graphics_draw_arc(ctx, bb_outer, GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(-135), DEG_TO_TRIGANGLE(135));
}

// Horizon-sun glyph: half-disc above a horizon line with three rays.
// Reuses the existing helper without the up/down arrow used by
// sunrise/sunset. Suitable as the title icon for the Sun Cycle card.
void icon_draw_horizon_sun(GContext *ctx, GPoint c, int size, GColor color) {
  draw_half_sun_with_rays(ctx, c, size, color);
}

// ---------- animated variants ----------

static void icon_draw_sun_rotating(GContext *ctx, GPoint c, int size, GColor color, uint32_t frame) {
  int r_disc = size / 4;
  int r_ray_in = size / 4 + 2;
  int r_ray_out = size / 2;
  set_stroke(ctx, color, 3);
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, c, r_disc);
  // 8 rays, rotating: each frame advances the angle by ~3 deg.
  int32_t base = (TRIG_MAX_ANGLE / 120) * (int32_t)(frame % 120);
  for (int i = 0; i < 8; ++i) {
    int32_t a = base + TRIG_MAX_ANGLE * i / 8;
    GPoint p1 = {
      c.x + (int16_t)(sin_lookup(a) * r_ray_in / TRIG_MAX_RATIO),
      c.y - (int16_t)(cos_lookup(a) * r_ray_in / TRIG_MAX_RATIO)
    };
    GPoint p2 = {
      c.x + (int16_t)(sin_lookup(a) * r_ray_out / TRIG_MAX_RATIO),
      c.y - (int16_t)(cos_lookup(a) * r_ray_out / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, p1, p2);
  }
}

static int bob_y(uint32_t frame, int amp) {
  // Slow vertical sine bob with period ~32 frames (~3.2s).
  int32_t a = TRIG_MAX_ANGLE * (int32_t)(frame % 32) / 32;
  return (int)(sin_lookup(a) * amp / TRIG_MAX_RATIO);
}

static void icon_draw_cloud_rain_animated(GContext *ctx, GPoint c, int size,
                                          GColor cloud_color, GColor drop_color,
                                          uint32_t frame) {
  int by = bob_y(frame, 1);
  GPoint cloud_c = { c.x, c.y - size/8 + by };
  icon_draw_cloud(ctx, cloud_c, size * 7 / 8, cloud_color);
  set_stroke(ctx, drop_color, 2);
  int spacing = size / 4;
  // Drops fall: each drop's y cycles through a short fall.
  int fall_top = c.y + size/5;
  int fall_bot = c.y + size/2;
  int range = fall_bot - fall_top;
  for (int i = -1; i <= 1; ++i) {
    int phase = ((int)frame + i * 4) % 8;
    int y_off = (phase * range) / 8;
    GPoint p1 = { c.x + i * spacing, fall_top + y_off };
    GPoint p2 = { c.x + i * spacing - 2, fall_top + y_off + 4 };
    graphics_draw_line(ctx, p1, p2);
  }
}

static void icon_draw_snow_animated(GContext *ctx, GPoint c, int size,
                                    GColor cloud_color, GColor flake_color,
                                    uint32_t frame) {
  int by = bob_y(frame, 1);
  icon_draw_cloud(ctx, GPoint(c.x, c.y - size/6 + by), size*3/4, cloud_color);
  // Three flakes drift down with horizontal sway.
  int fall_top = c.y + size/8;
  int fall_bot = c.y + size/2;
  int range = fall_bot - fall_top;
  for (int i = 0; i < 3; ++i) {
    int phase = ((int)frame + i * 5) % 12;
    int y_off = (phase * range) / 12;
    int sway = bob_y(frame + i * 3, 2);
    GPoint p = { c.x + (i - 1) * (size/5) + sway, fall_top + y_off };
    set_stroke(ctx, flake_color, 2);
    graphics_draw_line(ctx, GPoint(p.x - 3, p.y), GPoint(p.x + 3, p.y));
    graphics_draw_line(ctx, GPoint(p.x, p.y - 3), GPoint(p.x, p.y + 3));
  }
}

static void icon_draw_storm_animated(GContext *ctx, GPoint c, int size,
                                     GColor cloud_color, GColor bolt_color,
                                     uint32_t frame) {
  int by = bob_y(frame, 1);
  icon_draw_cloud(ctx, GPoint(c.x, c.y - size/6 + by), size*7/8, cloud_color);
  // Lightning flashes for 2 frames every 16.
  if ((frame % 16) < 3) {
    icon_draw_lightning(ctx, GPoint(c.x, c.y + size/4), size/2, bolt_color);
  }
}

static void icon_draw_fog_animated(GContext *ctx, GPoint c, int size,
                                   GColor color, uint32_t frame) {
  set_stroke(ctx, color, 3);
  int half = size / 2;
  int gap = size / 5;
  int drift = bob_y(frame, 4);
  graphics_draw_line(ctx, GPoint(c.x - half + 2 + drift, c.y - gap),
                          GPoint(c.x + half - 2 + drift, c.y - gap));
  graphics_draw_line(ctx, GPoint(c.x - half + 4 - drift, c.y),
                          GPoint(c.x + half - 4 - drift, c.y));
  graphics_draw_line(ctx, GPoint(c.x - half + 2 + drift/2, c.y + gap),
                          GPoint(c.x + half - 2 + drift/2, c.y + gap));
}

void icon_draw_condition_animated(GContext *ctx, GPoint c, int size,
                                  WeatherCondition cond, uint32_t frame) {
  int by = bob_y(frame, 2);
  switch (cond) {
    case COND_SUNNY:
      icon_draw_sun_rotating(ctx, c, size, ICON_SUN_COLOR, frame); break;
    case COND_PARTLY_CLOUDY: {
      icon_draw_sun_rotating(ctx, GPoint(c.x - size/4, c.y - size/4),
                             size*3/4, ICON_SUN_COLOR, frame);
      icon_draw_cloud(ctx, GPoint(c.x + size/6, c.y + size/8 + by),
                      size*3/4, ICON_CLOUD_COLOR);
      break;
    }
    case COND_CLOUDY:
      icon_draw_cloud(ctx, GPoint(c.x, c.y + by), size, ICON_CLOUD_COLOR); break;
    case COND_RAIN:
      icon_draw_cloud_rain_animated(ctx, c, size,
                                    ICON_CLOUD_COLOR, ICON_DROP_COLOR, frame);
      break;
    case COND_SNOW:
      icon_draw_snow_animated(ctx, c, size,
                              ICON_CLOUD_COLOR, ICON_DROP_COLOR, frame);
      break;
    case COND_STORM:
      icon_draw_storm_animated(ctx, c, size,
                               ICON_STORM_CLOUD, ICON_BOLT_COLOR, frame);
      break;
    case COND_FOG:
      icon_draw_fog_animated(ctx, c, size, ICON_FOG_COLOR, frame); break;
  }
}

// ---------- Phase 5/6/7/8 utility icons ----------

void icon_draw_clock(GContext *ctx, GPoint c, int size, GColor color) {
  int r = size / 2;
  set_stroke(ctx, color, 2);
  GRect circ = GRect(c.x - r, c.y - r, r * 2, r * 2);
  graphics_draw_circle(ctx, c, r);
  // Hour hand pointing up-right (~10 o'clock = -60deg).
  graphics_context_set_stroke_width(ctx, 2);
  int h_len = r * 5 / 8;
  int32_t a_h = DEG_TO_TRIGANGLE(-60);
  graphics_draw_line(ctx, c, GPoint(
      c.x + (int16_t)(sin_lookup(a_h) * h_len / TRIG_MAX_RATIO),
      c.y - (int16_t)(cos_lookup(a_h) * h_len / TRIG_MAX_RATIO)));
  // Minute hand pointing up.
  int m_len = r - 2;
  graphics_draw_line(ctx, c, GPoint(c.x, c.y - m_len));
  (void)circ;
}

void icon_draw_calendar(GContext *ctx, GPoint c, int size, GColor color) {
  int w = size;
  int h = size * 7 / 8;
  GRect body = GRect(c.x - w/2, c.y - h/2 + 2, w, h - 2);
  set_stroke(ctx, color, 2);
  graphics_draw_round_rect(ctx, body, 2);
  // Two top tabs.
  int tab_h = 4;
  graphics_draw_line(ctx, GPoint(c.x - w/4, c.y - h/2 - 1),
                          GPoint(c.x - w/4, c.y - h/2 + tab_h));
  graphics_draw_line(ctx, GPoint(c.x + w/4, c.y - h/2 - 1),
                          GPoint(c.x + w/4, c.y - h/2 + tab_h));
  // Header line.
  int hdr_y = c.y - h/2 + tab_h + 4;
  graphics_draw_line(ctx, GPoint(body.origin.x + 2, hdr_y),
                          GPoint(body.origin.x + body.size.w - 2, hdr_y));
  // A single filled "today" dot to suggest schedule.
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, GRect(c.x - 2, c.y + 1, 4, 4), 1, GCornersAll);
}

void icon_draw_moon_small(GContext *ctx, GPoint c, int size, GColor color) {
  // Crescent: filled moon disc with bg-color disc offset to carve a crescent.
  // Small header version uses the foreground color for the moon and lets
  // the cut-out be implied by stroke (no bg blanking — header is on top
  // of the card-bg fill so a 1px gap reads fine).
  graphics_context_set_fill_color(ctx, color);
  int r = size / 2;
  graphics_fill_circle(ctx, c, r);
  // Carve crescent by drawing a smaller circle in (near) bg color.
  // Since we don't know bg here, approximate with stroke-cut: draw a
  // thick arc in the same color over the right side, leaving a gap.
  // For a recognizable crescent at small sizes, use a filled square in
  // GColorClear-ish trick: fall back to drawing an unfilled ring.
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
}

// Integer-sqrt helper for scanline math. r <= ~64 in practice so this
// is plenty fast and avoids pulling in libm.
static int prv_isqrt(int v) {
  if (v <= 0) return 0;
  int x = v / 2 + 1;
  for (int i = 0; i < 8; ++i) {
    x = (x + v / x) / 2;
  }
  return x;
}

// Draws the moon disc with a phase shadow computed from the actual
// illumination percentage.
//
// The terminator (light/shadow boundary) on a sphere viewed from earth
// is a half-ellipse whose horizontal semi-axis is |1 - 2f|*r where f
// is the lit fraction. We render scanline-by-scanline:
//   for each y in [-r, r]:
//     x_moon = sqrt(r^2 - y^2)         // moon's half-width at this row
//     x_term = ((100 - 2*illum)/100) * x_moon  // terminator x (signed)
//   waxing  -> shadow covers [-x_moon, x_term] on this row
//   waning  -> shadow covers [-x_term, x_moon] on this row (mirrored)
// This keeps the shadow precisely clipped to the moon disc — no
// off-disc overflow, no fixed-per-phase fudge factors. The lit
// fraction matches `illum` exactly.
void icon_draw_moon_phase(GContext *ctx, GPoint c, int size,
                          uint8_t phase, uint8_t illum,
                          GColor moon_color, GColor bg_color) {
  int r = size / 2;

  // Body disc.
  graphics_context_set_fill_color(ctx, moon_color);
  graphics_fill_circle(ctx, c, r);

  // Full moon — no shadow.
  if (phase == 4 || illum >= 100) {
    return;
  }
  // New moon — entire disc shadowed; outline body so it stays visible.
  if (phase == 0 || phase == 8 || illum == 0) {
    graphics_context_set_fill_color(ctx, bg_color);
    graphics_fill_circle(ctx, c, r);
    graphics_context_set_stroke_color(ctx, moon_color);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_circle(ctx, c, r);
    return;
  }

  bool waxing = (phase >= 1 && phase <= 3);
  // Phase 5..7 are waning. If phase is out-of-range, fall back to
  // waxing so we still render something coherent.

  // Clamp illum into a sensible range for the math below.
  int f = illum;
  if (f < 1)   f = 1;
  if (f > 99)  f = 99;

  graphics_context_set_fill_color(ctx, bg_color);
  for (int y = -r; y <= r; ++y) {
    int x_moon = prv_isqrt(r * r - y * y);
    // Signed terminator offset. Positive when illum < 50 (shadow
    // covers > half), negative when illum > 50 (shadow is small).
    int x_term = ((100 - 2 * f) * x_moon) / 100;

    int x_left, x_right;
    if (waxing) {
      // Shadow on the left side of the moon, ending at the terminator.
      x_left  = -x_moon;
      x_right = x_term;
    } else {
      // Mirror for waning: shadow on the right side.
      x_left  = -x_term;
      x_right = x_moon;
    }
    if (x_right <= x_left) continue;
    graphics_fill_rect(ctx,
        GRect(c.x + x_left, c.y + y, x_right - x_left + 1, 1),
        0, GCornerNone);
  }

  // Re-stroke moon edge so a clean outline frames the disc on either
  // theme regardless of how thin the lit sliver is.
  graphics_context_set_stroke_color(ctx, moon_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, c, r);
}

void icon_draw_settings_gear(GContext *ctx, GPoint c, int size, GColor color) {
  int r_outer = size / 2;
  int r_inner = size / 4;
  set_stroke(ctx, color, 2);
  graphics_context_set_fill_color(ctx, color);
  // 6 small teeth around perimeter.
  for (int i = 0; i < 6; ++i) {
    int32_t a = TRIG_MAX_ANGLE * i / 6;
    int x = c.x + (int16_t)(sin_lookup(a) * r_outer / TRIG_MAX_RATIO);
    int y = c.y - (int16_t)(cos_lookup(a) * r_outer / TRIG_MAX_RATIO);
    graphics_fill_rect(ctx, GRect(x - 2, y - 2, 4, 4), 0, GCornerNone);
  }
  graphics_draw_circle(ctx, c, r_outer - 3);
  graphics_draw_circle(ctx, c, r_inner);
}

void icon_draw_lock_small(GContext *ctx, GPoint c, int size, GColor color) {
  int w = size * 3 / 4;
  int body_h = size / 2;
  int body_y = c.y;
  set_stroke(ctx, color, 2);
  graphics_context_set_fill_color(ctx, color);
  // Body.
  GRect body = GRect(c.x - w/2, body_y - body_h/2 + 2, w, body_h);
  graphics_fill_rect(ctx, body, 1, GCornersAll);
  // Shackle (arc above body).
  int sh_w = w - 4;
  GRect arc_bb = GRect(c.x - sh_w/2, body_y - size/2 - 1, sh_w, size/2 + 4);
  graphics_draw_arc(ctx, arc_bb, GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(-90), DEG_TO_TRIGANGLE(90));
  graphics_draw_arc(ctx, arc_bb, GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(-180), DEG_TO_TRIGANGLE(-90));
}
