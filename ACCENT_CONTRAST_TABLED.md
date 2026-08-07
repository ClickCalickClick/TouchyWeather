# Tabled: light-theme accent contrast

**Status:** tabled by decision, 2026-08-07. Not scheduled. Do not start without asking.

Reported by u/wickedest-witch on the r/pebble TouchyWeather 2.0 thread:

> the light version has some problems with lack of contrast (in particular, the
> orange-y color used for the high temperature & UV text banner doesn't always
> show up well because the screen itself leans a bit yellow when the backlight
> is off)

Recording the measurements so the next attempt starts from numbers instead of
re-deriving them.

## The defect is bigger than the report says

Badges are **a filled rounded rect with background-coloured text**
(`clock_zone.c`, `prv_draw_badge_row` / the comment at the pill drawer). In the
light theme that means **white text on `#FFAA00`**. It is not a soft edge — the
text itself is at the floor.

| Surface | Colour | Contrast (light theme) |
|---|---|---|
| High temp text on white | `GColorChromeYellow #FFAA00` | **1.9:1** |
| Low temp text on white | `GColorVividCerulean #00AAFF` | 2.6:1 |
| Badge pill — white text on the fill | `#FFAA00` | **1.9:1** |
| Rain alert banner fill | `#FFAA00` | 1.9:1 |

WCAG floors, for reference: 4.5:1 normal text, 3:1 large text. The orange fails
even the large-text bar. All four surfaces share one root, so one palette swap
fixes all four — darkening a pill's fill also fixes the white text sitting on it.

Both repos are affected: `theme.c` is duplicated app-and-watchface with identical
values. This is the "grep for the twin" rule in CLAUDE.md (three prior
occurrences). Fix one, fix both, or neither.

## Candidate palettes (measured, Pebble 64-colour palette)

| Option | Orange | Blue | Note |
|---|---|---|---|
| **Deep** | `GColorWindsorTan #AA5500` — 5.2:1 | `GColorCobaltBlue #0055AA` — 7.3:1 | Clears normal-text contrast everywhere. `#AA5500` reads *brown*: a real shift in the face's look. |
| **Mid** | `GColorOrange #FF5500` — 3.2:1 | `GColorBlueMoon #0055FF` — 5.6:1 | Still unmistakably orange/blue. Orange clears large-text only. |
| **Split** | Mid for accent TEXT, Deep for pill FILLS | | Best-looking; the fill needs the extra room because it carries white text. More code. |

White-on-fill, which is what the pill actually needs:
`#FFAA00` 1.9:1 → `#FF5500` 3.2:1 → `#AA5500` 5.2:1.

## The design that was agreed before it was tabled

- A Clay **radiogroup** `AccentStyle` — "Vivid (default)" / "Deep". Not a toggle:
  it is a palette choice, and "High contrast" as a label collides with Big Mode's
  meaning in the app.
- **Default stays Vivid.** Opt-in, explicitly — see the open question below.
- **Light theme only.** Deep on dark is strictly worse (`#AA5500` on black is
  3.2:1 where `#FFAA00` is ~10:1), so the swap gates on `s_mode == THEME_LIGHT`
  inside `theme_accent_*()`. This composes with night mode for free: the face
  already force-swaps to dark at night and remembers the daytime choice, so it
  reverts to vivid overnight with no extra logic.
- **Precedence in the app:** Big Mode already returns `theme_fg()`. Order is
  Big Mode → fg, else light+Deep → deep pair, else vivid.
- **Hidden on 1-bit.** diorite/flint already collapse accents to `theme_fg()`, so
  the item is a no-op there; hide it via the `activeWatchInfo.platform` branch
  already present in `clayCustomFn` for the card gate.
- Touches emery/gabbro rendering paths → another deliberate lock re-baseline.
  Defaults unchanged means *default rendering* stays pixel-identical, provable
  the same way round B was.

## Open question, deliberately unresolved

Shipping this purely opt-in means the reported defect ships **unfixed** — the
reporter would have to find a setting to get legible text, and 1.9:1
white-on-orange is not a taste preference.

The middle path that was proposed and not taken: keep Vivid the default for the
*text* accents (arguably taste) and fix the **badge pill fill unconditionally**,
since that is the surface she named and the one where the text is illegible.

Tabled without deciding. Revisit if more users report it.

## Related

Round C's other half — the clock-emphasis setting — is NOT a colour issue and
was not tabled. See the r/pebble backlog memory for what else is still owed from
that thread (WBGT, m/s wind, a reply to PeerDavid).
