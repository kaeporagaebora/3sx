# Parry System

This document describes how parrying works in the decompiled 3rd Strike code. The Japanese
name for parry is **blocking** (ブロッキング), which is why the code uses `blocking`, `paring`
(sic) and `block` throughout. Normal blocking (guarding) is called **guard**.

Some terms used below:

| Term in code | Meaning |
| --- | --- |
| `as` / `ds` | **A**ttacking / **D**efending work (the attacker and the defender) |
| `waza_flag[n]` | Per-command "input is live" countdown. Non-zero means the command is active |
| `reset[n]` | The value `waza_flag[n]` is loaded with when the command is input (the window length) |
| `hit_stop` / `dm_stop` | Freeze counters (hitstop). `dm_stop` is staged and copied into `hit_stop` later |
| `guard_chuu` | "Guarding in progress". Values 1–4 mean the player is in blockstun |
| `routine_no[0..3]` | State machine indices. For a player, `[1]` = 0 normal, 1 damage/guard, 2 catch, 3 caught, 4 attack |
| `kind_of_blocking` | 0 = normal parry, 1 = parry while already parrying ("rapid"), 2 = red parry (from blockstun) |

Frame numbers in this document use **frame N** for the frame on which the parry was detected
(the hit check), N+1 for the next frame, and so on.

There are two implementations in this repo: the **PS2** path (the default) and the **CPS3/arcade**
path (used when `ArcadeBalance_IsEnabled()` is true). They mostly agree; the differences are listed
in [PS2 vs arcade differences](#ps2-vs-arcade-differences).

---

## 1. Parry commands and input windows

Parries are ordinary entries in the command-input system (`cmd_main.c`), the same system that
reads special-move motions. Each character's command table (`cmd_data.c`, and
`arcade/arcade_cmd_data.c` for arcade) has the same five parry entries:

| Index | Table | Lever | `reset` | Used for |
| --- | --- | --- | --- | --- |
| 3 | `pc_cmd_03` | forward | 11 | Ground high parry |
| 4 | `pc_cmd_04` | down | 11 | Ground low parry |
| 5 | `pc_cmd_05` | forward | 8 | Air parry (only registers while airborne) |
| 6 | `pc_cmd_06` | down | 8 | Air "down" parry (only registers while airborne; arcade path only, see below) |
| 12 | `pc_cmd_12` | forward | 6 | "Anti-air" parry: ground high parry against attacks with `jump_att_flag` set |

Every entry uses command type 11, which dispatches to `check_10()` (`chk_move_jp[11]`). The lever
values are facing-relative (`pl_lvr_set()` swaps the 4/8 bits depending on facing), so 4 is always
forward and 8 is always back.

`cmd_data_set()` adds the System Direction **PARRY COMMAND INPUT TIME** option to `reset` for
all five entries: `blok_b_omake[] = { -2, 0, +2, +4 }` (index 1, +0, is the default).

### How long a window lasts

Each frame, `cmd_move()` runs every command's check function and then calls
`command_ok_move()`, which decrements every non-zero `waza_flag`. The hit check
(`hit_check_main_process()`) runs at the end of the frame, after all of that. So:

- On the frame you press the lever, the flag is set to `reset` and then decremented, so the hit
  check sees `reset - 1`.
- It counts down by 1 per frame and reaches 0 after `reset - 1` frames.

**The blue parry window is therefore `reset - 1` frames, starting with the frame of the input:**

| Parry | `reset` | Window (default setting) |
| --- | --- | --- |
| Ground high / low | 11 | **10 frames** |
| Air | 8 | **7 frames** |
| Anti-air (ground vs. `jump_att_flag` attacks) | 6 | **5 frames** |

### The input state machine (`check_10`)

Each parry command has its own small state machine (in `waza_work[player][index]`, with
`shot_ok` as the state):

- **State 0, wait for neutral.** Advance once the lever is neutral (`sw_lever == 0`).
- **State 1, wait for the press.** On a frame where a lever direction is *newly pressed*:
  - if the lever is exactly the command's direction (and, for 5/6, the player is airborne), the
    parry registers: `waza_flag = reset`, `free3 = reset + 10`, `w_int = 6`, go to state 2;
  - otherwise go back to state 0 (a press in any other direction resets the machine and you need
    neutral again).
- **State 2, still holding the lever.** Each frame `w_int--` and `free3--`:
  - lever released to neutral: go to state 3 (**window is kept**);
  - back pressed, or lever moved to any other direction: **window is cancelled** (`waza_flag = 0`), go to state 3;
  - `w_int` reaches 0 while forward is still held: **window is cancelled**, go to state 3.
- **State 3, cooldown after release.** Each frame `free3--`:
  - `free3 < 0`: the machine resets (`w_type = 0`, so `check_init()` runs next frame and returns to state 0);
  - forward (exactly the command's direction) or back newly pressed: **window cancelled**, stay in state 3;
  - any other newly pressed direction: **window cancelled**, go to state 4.
- **State 4.** Wait until `free3 < 0`, then reset.

What this means in practice:

1. **You must pass through neutral.** Going straight from back (blocking) to forward without a
   neutral frame doesn't register. Any newly pressed non-matching direction also sends the machine
   back to "wait for neutral".
2. **Tap, don't hold.** If you keep holding the lever, the window ends after 6 frames (the
   input frame plus 5) instead of the full 10. If you release within those frames, the full window
   stays. (Air parry: 7 frames tapped, 6 held. Anti-air: 5 either way.)
3. **Only the most recent direction is live.** When one parry command registers, it clears the
   other parry flags that have less time left (the `switch (waza_type[cmd_id])` blocks in
   `check_10`). Pressing another direction during state 2 or 3 also cancels the earlier window.
   A single forward press registers 3, 12 and (in the air) 5 together, which is intended: which
   one is checked depends on the situation.

### The input cooldown (lockout)

`free3` is the lockout timer. It starts at `reset + 10` on the input frame and counts down once
per frame starting on the next frame. The machine resets on the frame `free3` goes negative,
needs a neutral frame (state 0) after that, and only then accepts a new press. So:

- **The same parry direction can't be input again for `reset + 12` frames** after the input frame.
  With default settings that's **23 frames** for ground high/low, **20** for air and **18** for anti-air.
  The earliest frame a new press is accepted is input frame + `reset + 13`, and only if the lever
  was neutral on the frame before.
- **Re-tapping during the lockout is worse than doing nothing.** In state 3, pressing forward
  again cancels the window that is still running. Mashing forward kills your parry.
- The lockout is **per command**. Forward (3) and down (4) have separate machines, so a missed
  forward parry doesn't lock out a down parry. (Pressing down still cancels the forward
  window, as described above.)
- **Air forward (5) and air down (6) lock each other out.** When 5 registers, it's voided if
  command 6's `free3 > 0`, and the reverse is also true. Pressing down in the air therefore blocks air
  forward parries until command 6's lockout runs out.

### A successful parry clears the lockout

`set_paring_status()` calls `waza_compel_all_init2(ds)`, which sets `w_type = 0` on every command
the defender has. Next frame, `check_init()` resets every state machine to state 0 (`free3 = 0`).
**After a successful parry the defender can input the next parry right away**: return to
neutral, then tap again. There is no 23-frame lockout. This is what makes parrying multi-hit attacks
possible. (It also wipes any partly entered special-move motion.)

The leftover `waza_flag` from the previous input is *not* cleared, but it keeps counting down during
the freeze (see below). The freeze lasts at least 16 frames and the longest possible window is 14
frames (10 + the +4 option), so a leftover window always runs out before the attacker can move again.

---

## 2. Where a parry is decided (hit check)

When an attack's hitbox overlaps the defender, the hit check calls
`plef_at_vs_player_damage_union()` (player or projectile vs. player) or `set_caught_status()`.
These call:

- `defense_sky()` if the defender is airborne (`xyz[1].disp.pos > 0`, or a jumping `pat_status`)
- `defense_ground()` otherwise

They return **0 = parried** (go to `set_paring_status()`), **1 = guarded**
(`set_guard_status()`), or **2 = hit**. Parry is checked before guard.

### Preconditions (both paths)

A parry is only possible if all of these are true:

- `ds->py->flag == 0`: the defender is not dizzied.
- `!(ds->guard_flag & 2)`: `guard_flag` is 0 in normal states (standing, walking, jumping,
  crouching, in a parry) and in blockstun. It's 3 while the defender is attacking, in hitstun,
  or thrown, so you can't parry out of those. In the PS2 path, a dead player (`dead_flag`) gets
  `guard_flag = 3` too.
- The attack's `att.guard` bits allow it: bit `0x2` = can be parried high, `0x1` = can be parried
  low, `0x4` = can be air parried. An attack without these bits can't be parried that way.
- The matching System Direction switch is on (`DIP_UNKNOWN_8` / `DIP_UNKNOWN_9` = ground
  high/low parry disabled, `DIP_AIR_PARRY_DISABLED`, `DIP_ANTI_AIR_PARRY_DISABLED`,
  `DIP_RED_PARRY_DISABLED`).

### Blue vs. red: the `just_now` check

Both defense functions start with:

```c
if (ds->guard_chuu != 0 && ds->guard_chuu < 5) {
    just_now = 1;                                    // defender is in blockstun
    attr_att = check_normal_attack(as->wu.kind_of_waza);  // 1 = normal attack, 0 = special/super
}
```

`guard_chuu` is set every frame by the guard-stun routines (`Damage_04000` for standing/crouching
guard, routines 4–6, and `Damage_07000` for air guard, routine 7) to `guard_kind[routine - 4]` =
1–4. The parry states set it to 5–9 (`Normal_31000`/`Normal_35000`), and normal states set it to 0.
**A red parry is simply a parry while `guard_chuu` is 1–4, which means during blockstun,
including the guard freeze.**

### Ground (`defense_ground_ps2`)

For a high-parryable attack (`att.guard & 2`):

| Situation | Requirement |
| --- | --- |
| In blockstun (red) | `waza_flag[3] >= grdb[id][attr_att][0]` |
| Not in blockstun, attack has `jump_att_flag` | `waza_flag[12] != 0` and anti-air parry enabled |
| Not in blockstun, other attacks | `waza_flag[3] != 0` |

A low-parryable attack (`att.guard & 1`) works the same way with `waza_flag[4]` /
`grdb[id][attr_att][1]` (and for `jump_att_flag` attacks, `waza_flag[4]` gated by the anti-air
switch).

The success state is `routine_no[2]` = 31 or 32 for high (chosen by `check_attbox_dir()`) or 33
for low, and the attacker's hit flag gets `0x40`.

`jump_att_flag` comes from the attack data (`att.level & 0x40`, set in `charset.c`). It is not
"the attacker is jumping". It marks attacks that need the shorter **anti-air** window
(command 12, 5 frames).

### Air (`defense_sky_ps2`)

If the attack can be air parried (`att.guard & 4`):

| Situation | Requirement |
| --- | --- |
| In air blockstun (red) | `waza_flag[5] >= grdb2[id][attr_att]` |
| Otherwise | `waza_flag[5] != 0` |

Success sets `routine_no[2] = 34` (`0x22`) and hit flag `0x80`.

### Damage on a parry

`check_dm_att_blocking()` sets the parried damage to 0. The one exception is a projectile with
`work_id == 4 && id == 13` whose `TAMA.kz_blocking` is set: it does chip damage
(`dm_vital / kezuri_pow`). If that chip would KO, the parry fails (return 2) and the attack hits.
`set_paring_status()` also zeroes stun (`dm_piyo`).

---

## 3. Red parry timing

`cmd_data_set()` calls `make_red_blocking_time()` for commands 3, 4 and 5, after the
blue-window option has been added to `reset`:

```c
grdb[id][0][x] = reset - (blok_r_omake[r] + 2);   // vs. specials / supers
grdb[id][1][x] = reset - (blok_r_omake[r] + 3);   // vs. normals
grdb2[id][...]  = same formula, for air (command 5)
```

`blok_r_omake[] = { -1, 0, +1, +2 }` is the **RED PARRY C. INPUT TIME** option (default +0).

Red parry needs `waza_flag >= threshold` instead of just `!= 0`. On the input frame the flag is
`reset - 1` and it drops by 1 each frame, so the red window is the **first few frames of the same
input**:

| | vs. specials / supers | vs. normal attacks |
| --- | --- | --- |
| Red parry window | `r + 2` = **2 frames** | `r + 3` = **3 frames** |
| Blue parry window (same input) | 10 (ground) / 7 (air) | 10 (ground) / 7 (air) |

- Red parry windows are the same length on the ground and in the air, and they don't depend on the
  blue-window option (the option shifts both `reset` and the threshold by the same amount).
- "Normal attack" here means `sel_sp_ch_tbl[kind_of_waza >> 3] == 0`. Super art moves get bit `0x20`
  in `kind_of_waza`, so they count as specials.
- The anti-air command (12) has no red version. A red parry against a `jump_att_flag` attack
  uses command 3 (forward).
- Holding versus tapping doesn't matter for red parries, because the red window ends before the
  6-frame hold cutoff.

**Other differences between blue and red parries:**

- `blocking_point_count_up()` records `kind_of_blocking = 2` (red), 1 (parry while already in a
  parry state), or 0 (normal). This is used for grading (`grade_add_blocking`) and for the red
  afterimage: `effe5.c` swaps image 11 for image 33 when `kind_of_blocking == 2`.
- **Nothing else changes.** A red parry goes through the same `set_paring_status()`, so the
  freeze, recovery and meter gain are the same as a blue parry. The blockstun is thrown away
  (`routine_no[1]` goes from 1, damage/guard, to 0, normal) and the defender enters the normal
  parry state.

---

## 4. The parry freeze

### How the freeze lengths are chosen (`set_paring_status`)

```c
hsadix = 4;
if ((as->wu.kind_of_waza & 0xF8) == 0)          // plain normal attack (kind 0–7)
    hsadix = (as->wu.kind_of_waza / 2) & 3;

switch ((attacker airborne) + (ds->routine_no[2] - 31) * 2) {
case 0: case 2: case 4:          // ground parry, attacker on the ground
    ds->dm_stop  = -15;
    as->hit_stop = as->hit_quake = sel_hs_add_tbl[hsadix] + 16;
    break;
default (1,3,5,6,7,8,9):         // attacker airborne, or air parry
    ds->dm_stop  = -15;
    as->hit_stop = as->hit_quake = 16;
}
ds->dm_quake = 0;
```

`sel_hs_add_tbl[] = { 4, 3, 2, 1, 0, 0 }`.

**The attack's own hitstop values (`att.hs_me` / `att.hs_you`) are not used.** Parry freezes
are fixed:

| | Defender | Attacker |
| --- | --- | --- |
| Ground parry of a grounded normal (`kind_of_waza` 0–1 / 2–3 / 4–5 / 6–7) | 15 (animated, see below) | 20 / 19 / 18 / 17 |
| Ground parry of a grounded special or super | 15 | 16 |
| Ground parry of an airborne attacker | 15 | 16 |
| Air parry | 15 | 16 |

(`kind_of_waza / 2` most likely splits normals into strength pairs, so lighter normals give the
attacker a longer freeze. The table index is exact; the strength naming is inferred.)

The attacker also shakes for the same number of frames (`hit_quake`). The defender doesn't
shake (`dm_quake = 0`).

There is one exception. If the attack data has **both** `att.hs_me == 0` and `att.hs_you == 0`,
`set_paring_status()` skips all of this: the defender's `routine_no[2]` is restored from
`old_rno[2]`, so there is no freeze, no parry animation and no input reset. The damage is still
negated. This is how a hit with zero hitstop is handled.

### Positive vs. negative hitstop

`check_hit_stop()` (`plmain.c`) runs every frame before the player's state routine:

```c
if (hit_stop > 0) { hit_stop--; }                   // full freeze
else               { hit_stop++; char_move(&wu); }  // animation advances
...
return 1;   // the state routine (plmain_lv_02) is skipped this frame
```

- **Positive hitstop (attacker):** everything stops: logic, physics and animation.
- **Negative hitstop (defender):** logic and physics stop (the state routine, which does movement,
  doesn't run), but **the animation keeps playing** through `char_move()`. That's why the parry
  pose animates while the character stays in place.

Command input (`waza_check()`) and the state input handlers (`check_lever_data()` →
`process_normal()` → `nm_31000`/`nm_34000`) run **before** `check_hit_stop()` in
`Player_move()`/`player_mv_4000()`. **Inputs are read during the whole freeze**, for both
players.

### Frame-by-frame timeline

Starting from the hit check on frame N, which sets `ds->dm_stop = -15` and `as->hit_stop = 16 + b`
(where b is the bonus from `sel_hs_add_tbl`):

| Frame | Defender | Attacker |
| --- | --- | --- |
| N+1 | `hit_stop` is still 0, so the state routine runs once: `Normal_31000`/`Normal_35000` case 0 faces the attacker, starts the parry animation, adds meter, rumbles, and copies `dm_stop` into `hit_stop = -15` (`set_hit_stop_hit_quake`) | frozen (`hit_stop` 16+b → 15+b) |
| N+2 … N+16 | animated freeze (`hit_stop` -15 → 0). Inputs are read | frozen |
| N+17 | logic and physics resume | resumes if b = 0 (specials, supers, airborne attackers, air parries) |
| N+17+b | | resumes (grounded normals) |

**Against a special or super, both players resume on the same frame. Against a grounded normal,
the defender resumes `b` frames earlier (+4 to +1)**, on top of whatever recovery the attacker's
move has left.

If a second freeze arrives while one is already running (for example a projectile and a punch on
the same frame), `check_hit_stop()` merges them with `select_hit_stop()`: the larger magnitude
wins, and the sign of the counter being kept is preserved.

### Multi-hit attacks (e.g. Chun-Li's supers)

Every hit is checked and parried separately, and every parry restarts the fixed freeze from the
table above, no matter what hitstop the hit normally has. A super is not a plain normal (it has
the `0x20` bit), so **each parried hit freezes the attacker for 16 frames and the defender for 15
animated frames plus the setup frame.** After each freeze, the time until the next hit is however long the
attacker's animation takes to reach the next active hitbox. That comes from the move's animation
data in the character tables, not from the parry code.

What the defender has to do for each hit:

1. After a successful parry, all input machines reset (section 1), so there is **no lockout**.
2. Return the lever to neutral at least one frame (any time, freeze frames included).
3. Tap forward (or down for a low hit) so the window is still open when the next hit's hit
   check happens: within 10 frames before contact, counting the contact frame (2 frames for a red
   parry from blockstun vs. a super).
4. **Don't tap too early during the freeze.** If the tap's 10-frame window runs out before the
   next hit arrives, the lockout from section 1 applies (23 frames, and re-tapping cancels). The
   next hit can then only be blocked or taken.

If one of a super's hits has zero hitstop in its data (`hs_me == hs_you == 0`), that hit is
negated without a freeze and without the input reset. In that case the lockout from the input
you already made still applies.

---

## 5. What happens to movement during a parry

### Ground parry (`Normal_31000`, routines 31/32/33)

- Case 0 (frame N+1): faces the attacker (`rl_flag = (dm_rl + 1) & 1`, so parrying a cross-up
  turns you around), starts animation `routine_no[2] - 7`, applies the freeze, adds super meter
  (`add_sp_arts_gauge_paring`), rumbles.
- Cases 1/2 (after the freeze): play out the parry animation. There is no movement; the ground
  parry is stationary.
- **Acting out of a ground parry (`nm_31000`):** from frame N+2 on (once `routine_no[3]` is
  non-zero) and while the animation's `cg_type == 0`, the handler accepts EX/super arts, specials,
  taunt, throw, leap attack and normals. This runs during the freeze. The move is selected right
  away: `hissatsu_setup_union()` and similar only change `routine_no` to the attack state. The move's
  routine (`Player_attack`) starts on the first frame after the freeze. When the animation reports
  `cg_type` 64 or `0xFF`, the player returns to neutral (stand or crouch).
- Guarding while in a parry state works (`guard_flag` is 0), and so does parrying again
  (`kind_of_blocking = 1`).

### Air parry (`Normal_35000`, routine 34)

Jump momentum is stored in `wu.mvxy`: `a[]` is velocity and `d[]` is acceleration (gravity on the
Y axis). Position is only changed by `add_mvxy_speed()`, called from `jumping_union_process()`.

**Frame N+1 (case 0):**

1. Applies the freeze (`hit_stop = -15`).
2. **Facing:** if the player has to turn around to face the attacker, it flips `rl_flag` and also
   negates `mvxy.a[0]` and `mvxy.d[0]`. X velocity is applied relative to facing, so this keeps
   the player **moving in the same world direction**.
3. **Vertical velocity (`remake_mvxy_PoSB`):**
   ```c
   if (a[1].sp < 0)                      // falling
       a[1].sp = -(a[1].sp * 30 / 100);  // becomes 30% of the fall speed, upward
   ```
   If the player was **rising**, the vertical velocity is **unchanged**. If they were **falling**, it
   becomes a small upward hop (30% of the fall speed). Gravity (`d[1]`) is untouched.
4. Horizontal velocity and acceleration are **unchanged** apart from the facing fix.
5. Starts air parry animation 27, adds meter, rumbles. **No position update this frame.**

**Frames N+2 … N+16:** negative hitstop, so the state routine doesn't run and
`add_mvxy_speed()`/`cal_mvxy_speed()` aren't called. **The player hangs in the air**, with velocity
saved in `mvxy` and the parry animation playing.

**Frame N+17 and on (cases 1/2):** `jumping_union_process(wk, 3)` runs again: position +=
velocity, velocity += gravity. **The jump arc resumes from where it stopped**, with the
modified Y velocity. Landing (`xyz[1] + cg_jphos <= 0`) sets `routine_no[3] = 3` and plays the
landing animation.

All in all, the player doesn't move from frame N+1 through N+16 (16 frames). Then
they continue with the same horizontal speed and either the same upward speed or a small upward
hop in place of their fall.

**Acting out of an air parry (`nm_34000`):** unlike the ground parry, there are **no attack checks**
in the air parry state. The player gets control back when the animation reports `cg_type` 64 or
`0xFF` (`TO_nm_18000_01`, back to the normal jump state, where air attacks are allowed) or when
they land (`TO_nm_36000`/`37000`). The animation keeps playing during the freeze, so the moment
this happens depends on the air parry animation's length in the character data.

### What happens to an attacker whose jump-in is parried

In `set_paring_status()`, if the attacker is a player in a jumping `pat_status` (14–30) and the
move is a normal, `remake_mvxy_PoGR(&as->wu)` changes the attacker's momentum right away (they
still freeze for 16 frames before it takes effect):

- Vertical (only if gravity is active): rising → 80% speed; falling or zero → 10% speed.
- Horizontal: moving backward → 30% speed. Moving forward or not moving → **half speed (at least 1),
  reversed**, with horizontal acceleration cleared. The attacker drifts back away from the
  defender.

---

## 6. Summary of timing differences

| | Ground blue | Anti-air blue | Air blue | Red (ground or air) |
| --- | --- | --- | --- | --- |
| Command | 3 (fwd) / 4 (down) | 12 (fwd) | 5 (fwd) | 3 / 4 / 5 |
| Window, tapped | 10 f | 5 f | 7 f | 3 f vs normals, 2 f vs specials/supers |
| Window, held | 6 f | 5 f | 6 f | same as tapped |
| Same-direction lockout after a miss | 23 f | 18 f | 20 f | (uses the 3/4/5 lockout) |
| Defender freeze | 15 (animated) + 1 setup frame | same | same | same |
| Attacker freeze | 16 (+4…+1 for grounded normals) | 16 | 16 | as for blue |
| Can act during freeze | Yes, attacks are accepted and start when the freeze ends | Yes | No, waits for the animation to end or for landing | Same as the state you end up in |

All windows assume the default System Direction settings (both input-time options on their
second setting).

---

## 7. PS2 vs. arcade differences

The PS2 path is the default. The arcade path is used with `ArcadeBalance_IsEnabled()`.

| | PS2 (`defense_*_ps2`) | Arcade (`defense_*_cps3`) |
| --- | --- | --- |
| Air red parry | Yes (`grdb2` threshold) | **No**: in air blockstun, any `waza_flag[5] != 0` parries (blue window) |
| Air "down" parry (command 6) | Registered but never checked | Checked after command 5. Sets routine 35 (`0x23`), gated by `DIP_ANTI_AIR_PARRY_DISABLED` |
| Ground vs. `jump_att_flag` attacks | Command 12 (high) / 4 (low), **both gated by `DIP_ANTI_AIR_PARRY_DISABLED`**, with no fallback to command 3 | Command 12 for high, ungated. The low check uses command 4 without looking at `jump_att_flag` |
| Auto-parry (training, `DIP_AUTO_PARRY_DISABLED` cleared) | Every parry check passes | Not supported |
| `Normal_31000`/`35000` setup | Flips a positive `dm_stop` to negative and applies `subtract_dm_vital` (parry chip) | Leaves both alone |
| `add_sp_arts_gauge_paring` | No meter while a super freeze (`sa_stop_check`) is active | Always adds |

**ANTI-AIR PARRY default:** commit `03c5d7a6` (#287) set this System Direction switch to OFF in
`Dir_Default_Data` (`menu/dir_data.c`). `get_system_direction_parameter()` then set
`DIP_ANTI_AIR_PARRY_DISABLED` for every mode. In the PS2 path, that meant attacks with
`jump_att_flag` set couldn't be parried from the ground at all. In the arcade path, it turned off
the command-6 air parry. The default is now back to ON.

---

## Source reference

| What | Where |
| --- | --- |
| Parry command entries | `cmd_data.c` `pc_cmd_03/04/05/06/12`, `arcade/arcade_cmd_data.c` |
| Window option, red thresholds | `cmd_main.c` `cmd_data_set()`, `hitcheck.c` `make_red_blocking_time()`, `system/sysdir.c` `blok_b_omake` / `blok_r_omake` |
| Input state machine, lockout | `cmd_main.c` `check_10()`, `command_ok_move()`, `check_init()` |
| Input reset after a parry | `cmd_main.c` `waza_compel_all_init2()` |
| Parry vs. guard decision | `hitcheck.c` `defense_ground_ps2/cps3()`, `defense_sky_ps2/cps3()` |
| Red/rapid classification | `hitcheck.c` `blocking_point_count_up()` |
| Freeze lengths | `hitcheck.c` `set_paring_status()`, `sel_hs_add_tbl` |
| Freeze processing | `plmain.c` `check_hit_stop()`, `select_hit_stop()`, `pls02.c` `set_hit_stop_hit_quake()` |
| Parry states | `plpnm.c` `Normal_31000()` (ground), `Normal_35000()` (air) |
| Acting out of a parry | `pls00.c` `nm_31000()`, `nm_34000()` |
| Momentum changes | `pls02.c` `remake_mvxy_PoSB()` (air parry), `remake_mvxy_PoGR()` (parried jump-in) |
| Blockstun / `guard_chuu` | `plpdm.c` `Damage_04000()`, `Damage_07000()`, `guard_kind` |
| System Direction switches | `system/sysdir.c` `get_system_direction_parameter()`, `menu/dir_data.c` |
