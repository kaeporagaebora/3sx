# KO Freeze, Slow Motion and the Super Art Finish

This document describes what happens to time when a round ends by KO: the hitstop on the final
hit, the shake, the slow motion that eases back to normal speed, and the background effect on a
Super Art KO (the "dawn" background that fades to white).

It builds on the hitstop mechanics described in [parry.md](parry.md#positive-vs-negative-hitstop).
The same terms are used:

| Term in code | Meaning |
| --- | --- |
| `as` / `ds` | Attacker / defender |
| `hit_stop` / `dm_stop` | Freeze counters. `dm_stop` is staged by the hit check and copied into `hit_stop` by the defender's damage routine |
| `hit_quake` / `dm_quake` | Shake counters, staged and copied the same way |
| `att.hs_me` / `att.hs_you` | Hitstop for the attacker / defender, from the attack data |
| `dead_flag` | Set on the player whose vitality went below 0 |
| `SLOW_timer`, `SLOW_flag`, `EXE_flag` | Slow-motion controller (`engine/slowf.c`) |
| `pcon_rno[]` | Player-control state. `pcon_rno[0]` = 1 while fighting, 2 once the round is settled |
| `Round_Result` | Bit flags describing how the round ended |
| "akebono" (曙) | "Dawn". The code's name for the Super Art finish background |

Frame numbers use **frame N** for the frame on which the KO hit is detected (the hit check at the
end of the frame), N+1 for the next frame, and so on. "Real frames" means 60 Hz display frames.
"Logic frames" means frames on which the player logic actually runs.

---

## 1. Overview

A KO is made of four independent mechanisms layered on top of each other:

1. **The KO pause.** The KO text effect (effect 84) sets `Game_pause = 1` for **90 frames**
   (1.5 seconds) while "K.O." is on screen. This freezes both players, most effects, the stage, the
   gauges and the slow-motion timer. See [section 3](#3-the-ko-pause-ko-text).
2. **The KO hit's normal hitstop and shake.** These come from the attack data, like on any other
   hit. They are set up before the pause but mostly play out **after** it. Apart from the KO pause,
   there's no special hitstop for a KO hit (with one exception, see
   [same_dm_stop](#the-ko-hit-itself)).
3. **Global slow motion.** When the defender's vitality drops below zero, a 95-frame timer starts.
   It makes the player logic (and most effects, the stage and the gauges) run only on every 4th
   frame, then every 3rd, then every 2nd, then at full speed. The timer doesn't run during the KO
   pause. **The hitstop and shake from step 2 only count down on frames that run**, so the freeze
   looks about 4 times longer than normal.
4. **The Super Art finish effect** (Super Art KOs only). It is a full-screen effect that **runs at
   full speed**, unaffected by the KO pause and the slow motion: the dawn background, flashes, a
   fade to white, then the stage comes back.

What you see, matched to the code:

| What you see | Code |
| --- | --- |
| "K.O." on screen, everything frozen for about a second | `Game_pause = 1` for 90 frames (`eff84.c`) |
| KO scream when the text goes away | `dead_voice_request()`, called when the pause ends |
| "The KO'd opponent's sprite shakes a bit" | Defender's `hit_quake`, applied at ¼ speed after the pause and getting smaller as it runs out. The hitstop finishes at the same time |
| "Animations resume slowly, then get faster" | `SLOW_flag` stepping from 3 to 2 to 1 to 0 (¼ → ⅓ → ½ → full speed) |
| Dawn background, flashes, fade to white, disappears | Effect D3 `akebono_finish()`, 120 real frames, full speed, running mostly during the KO pause |

---

## 2. The KO hit itself

### Hitstop and shake for any hit

When a hit lands, `dm_status_copy()` (`hitcheck.c`) stages the freeze from the attack data:

```c
ds->dm_stop  = as->att.hs_you;     // defender freeze
ds->dm_quake = |as->att.hs_you|;   // defender shake
as->hit_stop = as->att.hs_me;      // attacker freeze, starts right away
```

Only the **defender** shakes. The attacker gets `hit_stop` but no `hit_quake`. (Parries are the
opposite: there, only the attacker shakes.)

`same_dm_stop()` then applies one KO-specific rule. If the attack has `att.dipsw & 1` and either
the defender is airborne or **the hit would KO** (`vital_new - dm_vital < -2`), the defender's
`dm_stop` is replaced with the attacker's `hs_me` (keeping the sign rules described in that
function). The result is that **on a KO hit, both players freeze for the same length** (for attacks
with that flag). The shake length stays `hs_you`.

The freeze lengths are per-hit values in the move data (`att.hs_me`, `att.hs_you`). They come from
the character tables, not from the C code, so the code can't tell you how long, say, the last hit
of a particular super freezes. What the code does decide is how much those frames get stretched
(sections 3 and 4).

### Frame N+1: the KO is registered

On the frame after the hit, the defender's `Player_damage()` runs once even though `dm_stop` is
set (a staged `dm_stop` with `hit_stop == 0` doesn't block the routine):

1. `get_damage_reaction_data()` → `subtract_dm_vital()` subtracts the damage. If `vital_new < 0`:
   ```c
   wk->vital_new = -1;
   wk->dead_flag = 1;
   dead_voice_flag = true;
   if (dm_guard_success != -1) kezurijini_flag = 1;   // KO by chip damage
   if (!round_slow_flag) {
       set_conclusion_slow();   // SLOW_timer = 95
       round_slow_flag = true;  // only once per round
   }
   ```
2. Because `dead_flag` is set, the hit reaction is converted to a KO reaction
   (`dd_convert[reaction][attack level]`, then `check_buttobi_type2()`). Usually this is a
   knockdown where the character flies through the air (the `buttobi` routines).
3. `dm_stop` is decremented by 1. At the end of `Player_damage()`, `set_hit_stop_hit_quake()` copies
   `dm_stop` into `hit_stop` and `dm_quake` into `hit_quake`.

Later in the same frame, `plcnt_move()` → `settle_check()` sees `dead_flag`:

- It picks `Winner_id` / `Loser_id` and runs the Super Art "resurrection" check
  (`check_sa_resurrection`, for supers that can bring a character back from a KO).
- It ORs the loser's `dm_koa` into `Round_Result`. `dm_koa` describes what kind of attack did the
  KO; see section 5.
- It calls `setup_settle_rno(0)`: `pcon_rno[0] = 2` (round over), `pcon_dp_flag = true`,
  `Conclusion_Flag = 1`. From now on, `Player_control()` runs `plcnt_die()` instead of
  `plcnt_move()`.
- It calls `request_center_message(0)` to show "K.O." (`message_index` 0; a double KO uses 1).
  This only happens when `Demo_Flag` is set, which it is during normal play. A Shun Goku Satsu KO
  doesn't request the message here; `syungoku_finish()` requests it later (section 6).
- Back in `plcnt_move()`: if `Round_Result & 0x980` (a Super Art or Shun Goku Satsu KO),
  **`effect_D3_init()` starts the finish effect on this same frame**.

### Trade KOs (both players hit on the same frame)

If both players hit each other on the same frame (`aiuchi_flag`), `plcnt_move()` applies the
damage with `subtract_dm_vital_aiuchi()` and then **overrides** the hitstop from the attack data:

| Result | `hit_stop` (both) | `hit_quake` (both) |
| --- | --- | --- |
| One player KO'd | 4 | 8 |
| Both players KO'd (double KO) | 2 | 4 |

The slow motion still starts. A trade KO therefore has a short fixed freeze; both players shake.

---

## 3. The KO pause (KO text)

`effect_84` (`effect/eff84.c`) is the center-message controller. It always exists during a match
and waits for `request_message`:

```c
const u8 Time_Data[5] = { 80, 90, 50, 50, 50 };

case 0:                                   // waiting
    if (request_message) {
        routine_no[0]++;
        dir_timer = Time_Data[request_message];   // request_message is always 1 -> 90
    }
case 1, sub-state 0:                      // next frame
    switch (message_index) {
    case 0: Game_pause = 1; effect_56_init(0, 0); break;   // "K.O."
    case 1: Game_pause = 1; effect_56_init(1, 0); break;   // "DOUBLE K.O."
    case 2: Game_pause = 1; effect_56_init(2, 0); break;   // time over (90 frames, or a special case on bonus stage 20)
    case 3, 4: no pause                                    // other messages
    }
case 1, sub-state 1:                      // every frame after that
    if (--dir_timer != 0) break;
    dead_voice_request();                 // KO scream
    request_message = 0;
    Game_pause = 0;
```

`request_center_message()` always sets `request_message = 1`, so the table index is always 1 and
the pause is always **90 frames**. `message_index` only picks which text is shown. (The other entries
of `Time_Data` are never selected by this code.)

**Double KO (since upstream #474):** for `message_index` 1, effect 84 sets `Game_pause = 1` but waits
until the combo counter display has finished (`cmb_all_stock[0] == 0`) before it shows "DOUBLE K.O."
and starts the 90-frame countdown. So a double KO that ends a combo pauses for longer than 90
frames.

### What `Game_pause` freezes

- **Players:** `Player_control()` skips the whole player update
  (`if (Game_pause || EXE_flag) goto end;`). That includes hitstop, shake, animation, physics and
  input. The sprites are still drawn, frozen in place.
- **Slow motion:** `set_EXE_flag()` does nothing while `Game_pause` is set. `SLOW_timer` doesn't
  count down and `EXE_flag` keeps its last value. The slow motion is put on hold and continues
  afterwards.
- **Most effects, the stage and the gauges:** the same `!EXE_flag && !Game_pause` checks as for slow
  motion.

What keeps running during the pause:

- The KO text itself (effect 56) and effect 84's own timer.
- The Super Art finish (effect D3) and its sparkles. Neither checks `Game_pause`.
- Sound.
- `Game_timer` (it only stops for `Game_pause == 0x81`, which is the pause menu).

### Timeline

| Real frame | What happens |
| --- | --- |
| N | KO hit lands (hit check). |
| N+1 | Full speed. KO registered (`dead_flag`, `SLOW_timer = 95`, the defender's `hit_stop`/`hit_quake` set, first shake offset applied), round settled, `request_center_message(0)`. Later in the frame, effect 84 picks up the request (`dir_timer = 90`). On a Super Art KO, effect D3 starts |
| N+2 | `set_EXE_flag()` still runs (no pause yet): `SLOW_timer` → 94, ¼ speed. The players run **only if** `Game_timer % 4 == 0` on this frame. Later in the frame, effect 84 sets **`Game_pause = 1`** and shows "K.O." |
| N+3 … N+92 | **Paused (90 frames).** Players frozen, slow-motion timer on hold. Only the KO text, the D3 finish and sound move |
| N+92 | `dir_timer` reaches 0: KO voice, `Game_pause = 0` |
| N+93 … N+185 | Slow motion continues from `SLOW_timer` = 93: ¼ speed, then ⅓, then ½. **The KO hit's hitstop and shake play here**, at ¼ speed |
| N+186 | Full speed |

So the order you see is: one full-speed frame (maybe two), then 1.5 seconds frozen with "K.O." on
screen, then the shake and the rest of the hitstop at ¼ speed, then the knockdown speeding up from
¼ to full speed.

---

## 4. Slow motion (`slowf.c`)

### The controller

```c
const s8 slow_timer_to_flag[32] = { 1,1,1,1,1,1, 2,2, 3,3,3,3,3,3,3,3, 3,... };

void set_conclusion_slow() { SLOW_timer = 95; }

void set_EXE_flag() {               // called once per frame at the start of Game2_1()
    if (!Game_pause) {
        if (SLOW_timer) {
            if (--SLOW_timer) {
                SLOW_flag = slow_timer_to_flag[min(SLOW_timer / 8, 31)];
            } else {
                SLOW_flag = 0;
            }
        }
        EXE_flag = Game_timer % (SLOW_flag + 1);
    }
}
```

`EXE_flag` is 0 on frames that should run and non-zero on frames that should be skipped. With
`SLOW_flag = s`, one frame out of every `s + 1` runs.

| `SLOW_timer` after decrement | `SLOW_timer / 8` | `SLOW_flag` | Logic runs | Real frames in this phase | Logic frames in this phase |
| --- | --- | --- | --- | --- | --- |
| 94 – 64 | 11 – 8 | 3 | every 4th frame (¼ speed) | 31 | 7–8 |
| 63 – 48 | 7 – 6 | 2 | every 3rd frame (⅓ speed) | 16 | 5–6 |
| 47 – 1 | 5 – 0 | 1 | every 2nd frame (½ speed) | 47 | 23–24 |
| 0 | — | 0 | every frame | — | — |

The exact number of logic frames per phase depends on the value of `Game_timer` when the KO
happens (which frames are multiples of 4, 3 or 2).

**In total, the slow motion covers 94 timer frames and fits about 35–38 logic frames into them.**
This is the "resume slowly" ramp: it starts at a quarter of normal speed and gets faster in two
steps.

`SLOW_timer` is set during frame N+1, after `set_EXE_flag()` has already run for that frame. So
frame N+1 runs at full speed. N+2 is the first slow frame. After that, the KO pause (section 3)
puts the timer on hold for 90 frames, so the remaining 93 timer frames run from N+93 to N+185.

### What is slowed down

Everything that checks `EXE_flag` only runs on frames where it is 0:

- **Both players.** `Player_control()` (`plcnt.c`) skips the whole player update during the
  fight and settle states (`pcon_rno[0] + pcon_rno[1] != 0`):
  ```c
  if (Game_pause || EXE_flag) goto end;
  ```
  That includes input reading, `check_hit_stop()` (so **hitstop only counts down on frames that
  run**), animation (`char_move`), physics, `set_quake()` (so **the shake only moves on frames that run**),
  hit detection requests and the round-end logic (`plcnt_die()`).
- **Most effects.** Hit sparks, dust and so on test `!EXE_flag && !Game_pause` in their move
  routines (`eff01.c`, `eff09.c`, `eff30.c`, `effe5.c` afterimages, and many more).
- **The stage.** Several stage routines (`stage/bg.c`, `bg090.c`) skip their updates when `EXE_flag` is set.
- **The gauges.** `vital.c`, `stun.c` and `spgauge.c` skip when `EXE_flag` is set.

What is **not** slowed down:

- The Super Art finish effect (D3) and its particles (G8). They never check `EXE_flag`, so they
  run at 60 Hz.
- Sound.
- `SLOW_timer` itself (otherwise the slow motion would also stretch itself out).

On skipped frames the player sprites are still drawn, at the position and animation frame from the
last frame that ran (`add_next_position()` isn't called again, so the shake offset `next_x` stays
where it was). That's why the slow motion looks like stepping rather than smooth movement.

### The freeze and the shake under slow motion

Example: the KO hit has `hs_you = H` (and, with `same_dm_stop`, the attacker freezes for the same
length).

| Real frame | What happens |
| --- | --- |
| N | Hit check. The attacker's `hit_stop` is set right away. The defender's `dm_stop` / `dm_quake` are staged |
| N+1 | **Full speed.** The defender's damage routine runs: KO registered, `SLOW_timer = 95`, KO reaction chosen, `hit_stop = H-1`, `hit_quake = H`. `set_quake()` applies the first (largest) shake offset. The round is settled and "K.O." requested. On a Super Art KO, effect D3 starts |
| N+2 | ¼ speed. The players get one logic frame only if `Game_timer % 4 == 0`. The KO pause starts at the end of this frame |
| N+3 … N+92 | **KO pause.** Everything frozen, including the shake offset |
| N+93 … | ¼ speed. On each frame that runs, `hit_stop` goes down by 1 and `hit_quake` goes down by 1 with a new offset from `quake_table`. On skipped frames, nothing moves |
| when `hit_stop` reaches 0 | The KO reaction (e.g. the knockdown flight) starts moving, still in slow motion |
| N+123 → N+185 | Speed goes up to ⅓ (N+123), then ½ (N+139) |
| N+186 | Full speed |

At ¼ speed, a freeze of H logic frames lasts about **4H real frames**. A freeze that would normally
be barely noticeable (say 10 frames) becomes about 40 real frames, roughly ⅔ of a second. That
comes on top of the 90-frame KO pause.

**The shake** (`set_quake()`, `plcnt.c`):

```c
if (hit_quake) {
    hit_quake--;
    next_x = quake_table[hit_quake];     // pixel offset, mirrored by facing
} else {
    next_x = 0;
}
```

`quake_table[64]` alternates sign on every entry, and its values get bigger as the index goes up
(±1 near the start of the table, ±2 in the middle, ±3 at the end). The counter counts down, so
the shake **starts wide and settles down**, flipping sides on every frame that runs. Under ¼ speed,
each position is held for 4 real frames, which makes the shake look slow and jerky. The shake count
is `hs_you`, while the defender's freeze may be `hs_me` (through `same_dm_stop`). If those differ,
the shake can end before the freeze does, or keep going after it ends.

Since upstream's change to `set_quake()`, a shake count of 64 or more is clamped to the last two
table entries (±3). The table has only 64 entries, so larger counts used to read past its end.

According to the code, the hitstop and the shake **start on the same frame**, N+1. But after one
or two frames they're frozen by the KO pause, so on screen the order is: frozen with "K.O.", then
shaking, then the slow-motion knockdown.

---

## 5. Which KOs get the Super Art finish

`dm_koa` (the defender's "kind of attack") comes from `acatkoa_table[kind_of_waza]`
(`charset.c`). `kind_of_waza` is a per-move value in the attack data.

| `kind_of_waza` | `koa` bit |
| --- | --- |
| 0–1 | `0x4` |
| 2–7 | `0x8` |
| 8–15 | `0x10` |
| 16–23 | `0x20` |
| 24–31 | `0x40` |
| 32–39, 48–55 | `0x80` (Super Art) |
| 40–47, 56–63 | `0x100` (Super Art) |
| 64 | `0x800` (Shun Goku Satsu) |

A few Super Arts that power up the character's moves while active (Yun, Yang, Makoto and Twelve,
plus one of Oro's Super Arts) also set `at_koa = 0x80` on the attacks they make (`plmain.c`).
So a KO with a powered-up move during the super counts as a Super Art KO.

The `Round_Result` bits involved:

| Bit | Meaning | Effect |
| --- | --- | --- |
| `0x80`, `0x100` | KO by a Super Art | Dawn finish effect (`effect_D3_init(0)`), "SA" win mark, music fade |
| `0x800` | KO by Shun Goku Satsu | `effect_D3_init(1)` if Akuma won (`syungoku_finish`), or the dawn effect otherwise |
| `0x200` | KO by chip damage (`kezurijini_flag`) | "Chip" win mark |
| `0x4` | Vitality drain KO (training/extra option) | |
| `0x1` | Time over | |

The **slow motion is the same for every kind of KO**: always `SLOW_timer = 95`, started once per
round. A Super Art KO only adds the D3 effect on top. Throw KOs start the same slow motion
(`plpca.c`). The vitality-drain option KO (`check_omop_vital()` in `plmain.c`) sets `dead_flag`
without calling `set_conclusion_slow()`.

---

## 6. The Super Art finish background (`effd3.c`, `akebono_finish`)

Effect D3 type 0 runs once per real frame, at full speed, for 120 frames. It has three parts:

- The dawn picture, drawn on stage background layer 3.
- A full-screen white panel drawn over it with `overwrite_panel(color, 69)`. `color` is ARGB, so
  `0x10FFFFFF` is white at alpha `0x10` (almost transparent) and `0xFFFFFFFF` is solid white.
- Sixteen sparkle particles (effect G8).

### Frame-by-frame

Counting from the frame the effect starts (F0 = the KO frame N+1). The timer works like this: a
step with timer value `t` is shown for `t + 1` frames.

| Frames | Phase (`routine_no[0]`) | Panel colour (`ake_color`) | Other |
| --- | --- | --- | --- |
| F0 | 0: setup | `0xFF` white | The stage layers listed in `ake_bg_off[stage]` are turned off, the dawn layer is turned on and positioned, sound effect 117 plays, `akebono_flag = 1` |
| F0–F4 | 1 | `0xFF` (solid) | Flash |
| F5–F9 | 1 | `0x10` (almost clear) | The dawn background is visible |
| F10–F14 | 1 | `0xFF` | Flash |
| F15–F19 | 1 | `0x10` | |
| F20 | 1 → 2 | `0xFF` (1 frame) | **`effect_G8_init()`**: 16 sparkles appear at random times, grow and fly outward |
| F21–F31 | 2 | `0x10` | The fade to white starts |
| F32–F42 | 2 | `0x28` | |
| F43–F53 | 2 | `0x40` | |
| F54–F64 | 2 | `0x60` | |
| F65–F75 | 2 | `0x88` | |
| F76–F86 | 2 | `0xB0` | |
| F87–F97 | 2 | `0xD8` | |
| F98 | 2 → 3 | `0xFF` | The dawn layer is turned off (`Bg_Off_R(8)`), hidden behind the white |
| F99–F118 | 3 | `0xFF` (solid) | At F118 the normal stage layers are turned back on, behind the white |
| F119 | 3 → 4 | — | `akebono_flag = 0`: no more panel, so the white **disappears in one frame**, showing the stage again. The G8 sparkles see `akebono_flag == 0` and remove themselves |
| F120 | 4 | — | The effect frees itself |

So the sequence is: **about 20 frames of flashing between white and the dawn picture, about 77
frames of fading to white in 7 steps of 11 frames, about 21 frames of solid white while the stage
is put back, then a cut back to the stage.**

The effect uses real frames and ignores both the KO pause and the slow motion. The two line up like
this (F0 = N+1):

| Real frames | Finish effect | Players |
| --- | --- | --- |
| N+1 → N+2 | Flashing | One full-speed frame, maybe one ¼-speed frame |
| N+3 → N+92 | Flashing, sparkles, most of the fade to white | **Frozen by the KO pause** ("K.O." on screen) |
| N+93 → N+119 | End of the fade, then solid white while the stage is put back | ¼ speed: KO hitstop and shake |
| N+120 | The white cuts out | Still ¼ speed, ⅓ from N+123, ½ from N+139, full from N+186 |

So on a Super Art KO, almost the whole dawn sequence plays during the KO pause, and the shake and
the slow-motion knockdown play behind (and just after) the solid white.

`akebono_flag` is also read by `ta_sub.c` and `eff77.c` to pause other background effects while the
finish is showing.

### Shun Goku Satsu (`syungoku_finish`, effect D3 type 1)

When Akuma wins with Shun Goku Satsu, the round is settled with `setup_settle_rno(4)`
(`settle_type_40000`) and D3 runs `syungoku_finish()`. It hides both players
(`disp_flag = 0`) and the hit marks, turns the stage layers off, draws a solid black panel
(`0xFF000000`, priority 70) with a background layer behind, and starts effects 20 and L9. Once
those report they're done (`old_rno[0]`), it shows the players again, turns the stage back on, and
posts the KO message itself (`request_center_message(0)`). `settle_type_40000` also calls
`set_conclusion_slow()` again when the winner starts their victory pose, so this KO gets a second
round of slow motion.

---

## 7. After the slow motion: settling the round

`plcnt_die()` keeps running the players (still subject to `EXE_flag`) and calls
`settle_type_00000()` (normal KO):

1. Wait until the loser has landed and is lying down (`nekorobi_check(Loser_id)`), or for 60 logic
   frames (`dir_timer`). Then lock the winner's input (`wkey_flag = 1`).
2. Once the winner is standing still (`footwork_check`), the winner switches to the victory routine
   (`routine_no[2] = 40`) and the loser to routine 41. `complete_victory_pause()` sets
   `Complete_Victory`.
3. Wait for the winner's victory animation (`routine_no[3] == 9`).

A double KO uses `settle_type_10000()` (wait for both players to land), and time over uses
`settle_type_20000()`. Time over doesn't start the slow motion.

---

## Source reference

| What | Where |
| --- | --- |
| Hitstop/shake from attack data | `hitcheck.c` `dm_status_copy()`, `same_dm_stop()` |
| Staged → active freeze | `pls02.c` `set_hit_stop_hit_quake()`, `plpdm.c` `Player_damage()` |
| Freeze countdown | `plmain.c` `check_hit_stop()` |
| Shake | `plcnt.c` `set_quake()`, `quake_table`, `add_next_position()` |
| KO detection, start of slow motion | `plpdm.c` `subtract_dm_vital()`, `subtract_dm_vital_aiuchi()`; `plpca.c` (throws) |
| KO reaction | `plpdm.c` `get_damage_reaction_data()`, `dd_convert` |
| Trade KO freeze | `plcnt.c` `plcnt_move()` (`aiuchi_flag` block) |
| Round settle, KO type | `plcnt.c` `settle_check()`, `setup_settle_rno()`, `settle_type_*()` |
| Slow-motion controller | `slowf.c` `set_conclusion_slow()`, `set_EXE_flag()`; called from `game.c` `Game2_1()` |
| Player update skipped in slow motion | `plcnt.c` `Player_control()` |
| KO-type table | `charset.c` `acatkoa_table` |
| Super Art finish | `effect/effd3.c` `akebono_finish()`, `syungoku_finish()`, `ake_timer_tbl`, `ake_color` |
| Sparkles | `effect/effg8.c` |
| White/black panel | `ui/sc_sub.c` `overwrite_panel()` |
| Win marks, music | `manage.c` `Setup_Win_Mark()` |
