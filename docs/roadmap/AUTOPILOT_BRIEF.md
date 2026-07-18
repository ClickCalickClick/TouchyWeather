# Overnight Autopilot Brief

**For the implementing agent.** The user (Jared) is asleep. Execute the roadmap in `docs/roadmap/` autonomously under the rules below. He will review everything in the morning.

## Ground rules

1. **Read first:** `00_MASTER_REPORT.md` (especially the gesture budget and sequencing), then each phase doc as you reach it. The docs are authoritative; where a doc and this brief conflict, this brief wins.
2. **Git:** all work on the local branch `feature/roadmap-phases` (branched from `release/1.11.1`). **One commit per task** (e.g. one for Task 1.1, one for 1.2+5.1, …). **Never push. Never merge.** Local commits only.
3. **DECISIONS.md:** maintain `docs/roadmap/DECISIONS.md`. Every judgment call the docs left open — a threshold you picked, a fallback you took, an API that didn't behave as documented, a scope trim — gets an entry: *what you chose, why, and how to reverse it.* This is the user's morning review sheet. When in doubt whether something is log-worthy: log it.
4. **Verification gates (from CLAUDE.md):** after each task, `pebble build`, install on the emulator, and take screenshots (`pebble screenshot --emulator <platform> --vnc --no-open`). **Look at the screenshot.** A task is done only when the screenshot shows the intended behavior and no regression. Do not commit unverified work.
5. **Quality over completion.** If a phase can't be finished cleanly, stop at the last green commit, write up where and why in DECISIONS.md, and move on **only if** later phases don't depend on it (check the master-report dependency graph). Never leave the branch in a non-building state at a commit boundary.
6. **No pushes, no releases, no external calls beyond what the app itself makes.** Do not touch the Vercel proxy deployment.

## Execution order

Phase 0 → Phase 1 (+ Task 5.1 rides along) → Phase 2 → Phase 3.2 → Phase 4 → Phase 3.1 Stage A → Phase 5 → Phase 3.1 Stage B (Big Mode).

Realistic overnight scope is roughly through Phase 4; Stage A and Phase 5 are large. Getting further is fine if gates stay green; stopping earlier at a clean commit is also fine.

## Decisions already made by the user (do not re-litigate)

- **Settings entry:** long-press BACK opens the Settings card (double-tap rejected). Single-BACK exit timing must not change. See `PHASE_2_NAV_REFACTOR.md`.
- **Settings parity:** anything togglable on the watch must also be editable in Clay — both surfaces, reconciled (Phase 2.2 hybrid sync is a hard requirement).
- **Platforms:** all 7 targets, graceful degradation; aplite is a measured lean no-go (44 KB binary > 24 KB App RAM) — build for the other six, don't fight aplite.
- **Phase 4:** all five forecast cards get detail modals, bottom-sheet style, per the rewritten `PHASE_4_DEEP_DIVE_UI.md`.
- **Gestures:** the master-report gesture budget is final.

## Known landmines (verified findings — trust these)

- Any `WeatherData` struct change ⇒ bump `PERSIST_KEY_CACHE` (107) in `comm.c`. Phase 4's UV/AQI additions land together as **one** bump.
- Clay values arrive as C-strings needing `atoi` (`comm.c:97+` pattern).
- gabbro is **260×260 round**; chalk (180×180 round) is a different, smaller screen class.
- Radar's 25.6 KB buffer likely doesn't fit 64 KB platforms — compile it out there per `PHASE_5_PLATFORM_EXPANSION.md` §2, then measure.
- `EnabledMask` message key exists in `package.json` but is referenced nowhere in `src/` — free to define its semantics.
- Keep the animation ticker alive while `refresh_sheet_is_active()` (Phase 1.1) or the spinner dies.

## Escalation protocol (the user is asleep — never wait for him)

Three tiers, in order of severity:

1. **Confused but unblocked** → make the best call, log it in DECISIONS.md, keep moving. This is the default; use it for ~everything.
2. **Critical decision fork** — irreversible choice, evidence contradicting the roadmap docs, or a design fork where a wrong guess wastes hours → spawn a **synchronous consult subagent with `model: "fable"`** (`run_in_background: false`). Give it: the focused question, the specific evidence/error, and paths to the relevant `docs/roadmap/*.md` files (it has no other context — the docs are its briefing). Treat its answer as the tiebreaker. Log the question, the answer, and what you did in DECISIONS.md. If the `fable` model override is unavailable on this plan, fall back to tier 1 and mark the entry **NEEDS REVIEW**.
3. **Emergency** — risk of damaging the repo, builds broken with no obvious path forward, corrupted state → **the user trusts Fable to solve it.** Spawn a synchronous `model: "fable"` consult with the full situation (what happened, exact errors, current git state, what you've tried) and **follow its recovery plan**. Log the whole exchange in DECISIONS.md under an **EMERGENCY** heading. Two absolute rails survive even here: **never push, and never run destructive commands outside the repo working tree** (the local branch history is the safety net — as long as commits exist, everything is recoverable). Only if Fable itself cannot produce a safe path: stop at the last green commit and leave the write-up.

Escalate sparingly: tier 2 should fire a handful of times a night at most, tier 3 ideally never — but when it does, solve it, don't wait for morning.

## User action items (cannot be autopiloted — leave in DECISIONS.md as reminders)

- **Rotate the radar-proxy shared secret** in Vercel (`tw-radar-prod-…`, exposed in the public repo at `src/pkjs/index.js` ~537–546). Phase 0's client-side cleanup happens on-branch, but rotation needs Jared's Vercel access — until he rotates, the old key remains live.
- Review DECISIONS.md, then decide what to push/PR.
