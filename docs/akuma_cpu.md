# Akuma's CPU

This document explains how the CPU-controlled Akuma (Gouki) decides what to do, based on the decompiled PS2
code. The CPU code lives in `src/sf33rd/Source/Game/com/`. Akuma's own scripts are in
`active/active14.c` (151 action patterns), `passive/pass14.c` (253 reaction patterns) and `shell/shell14.c`
(13 projectile reactions). Akuma is character number 14 in all the tables.

The full list of patterns, translated into readable steps, is in the [appendix](#appendix-every-pattern).

---

## 1. How the CPU thinks

The CPU produces a fake joystick input every frame (`cpu_algorithm` in `com_pl.c`). It's a state machine with
these main modes:

| Mode | What it does |
| --- | --- |
| **Active** | Akuma picks something to do on his own |
| **Passive** | Akuma reacts to something you just did |
| **Follow** | Akuma follows up after a hit, block or parry |
| **Guard / Flip** | Blocking, and parrying ("flip" in the code) |
| **VS Shell** | Dealing with your projectiles |

A **pattern** is a short script of steps, such as "walk in", "jump toward", "press cr.MK", "do a Gou
Hadouken" or "wait until the opponent lands". Most steps check for a reaction every frame
(`Check_Passive`), so if you do something he has an answer for, he drops the current script and switches to
a reaction. Some scripts start with an "ignore reactions until done" step (`Pierce_On`), which makes him
commit to the whole sequence.

Scripts also hit-confirm. Each attack step says what happens next:

| In the appendix | Meaning (`Reaction_Sub`) |
| --- | --- |
| *(continues only on hit)* | Carries on only if the attack hit you; ends if it was blocked, parried or whiffed |
| *(continues if it connects)* | Carries on if it hit, was blocked or was parried; ends if it whiffed |
| *(stops if blocked/parried)* | Ends if blocked or parried; carries on if it hit or whiffed |
| *(then counter-attack mode)* | Carries on regardless and is ready to punish |
| no note | Always carries on |

### Distance

Akuma's choices depend on the distance between you (`Area_Unit_Data[14]`):

| Range | Distance |
| --- | --- |
| Near | 0–71 |
| Mid | 72–127 |
| Far | 128–191 |
| Very far | 192+ |

For projectiles he uses wider bands (`Shell_Area_Unit_Data[14]`): 71 / 159 / 223.

### Difficulty level

Most choices come from tables indexed by a level from 0 to 7 (`Setup_Lv08`). The level comes from
`Control_Time`, which starts at 481 at the start of an arcade run. It goes up by 1 every second of play and
by 40 every time you win a round. The level steps up every 60 points: level 0 up to 541, level 7 above 901.
The arcade difficulty setting then shifts it (`emLevelRemake`): −2 levels on the easiest setting, +1 on
the second hardest and +4 on the hardest.

Akuma is fought late in a run, so in practice he's usually at **level 6 or 7**. The difficulty level
matters less than you might expect, though:

- **Action tables:** his levels 0–6 are identical, except one extra slot at mid range from level 6
  (a [Kongou Kokuretsuzan](#supers) try). Only **level 7** changes his choices.
- **Reaction tables:** these have 4 levels (0–3). Their entries are spread fairly evenly, so the level
  mostly changes the odds slightly.
- **Parry chance:** this goes up steadily with level (see [section 5](#5-parrying)).

His reaction time isn't special. Reaction delays come from `Reflection_Speed_Unit_Data`, and Akuma's rows
are the same as Ryu's and Ken's: about 9–10 frames on average at the lowest setting, falling to under 1 frame
at the highest.

### "Dramatic" moments

Each reaction roll picks 1 of 16 entries. If Akuma has super meter ready, or either player is below 49 life
(about 30%), the game only rolls from the **second half** of the table (`Check_Dramatic`). Those halves
are noticeably more aggressive, with more supers, Raging Demon attempts and Shoryukens. In the tables
below these are listed as "dramatic".

---

## 2. Akuma's moves, as the CPU sees them

The CPU performs special moves by replaying the motion from the command table (`pE_cmd` in
`engine/cmd_data.c`). Decoding those motions gives these names:

| Command | Motion | Move |
| --- | --- | --- |
| 31 | D, DF, F + P | Gou Hadouken |
| 33 | DF, D, DB, B + P | Shakunetsu Hadouken |
| 30 | F, D, DF + P | Gou Shoryuken |
| 32 | D, DB, B + K | Tatsumaki Zankukyaku |
| 35 | F, D, DF + K | Hyakkishu |
| 28 / 29 | F, D, DF / B, D, DB + PPP or KKK | Ashura Senku (forward / back) |
| 46 | (air) D, DF, F + P | Zanku Hadouken (air fireball) |
| 47 | (air) D, DB, B + K | Air Tatsumaki |
| 20 | QCF×2 + P | SA1 Messatsu Gou Hadou |
| 21 | QCF×2 + P | SA2 Messatsu Gou Shoryu |
| 22 | QCF×2 + K | SA3 Messatsu Gou Rasen |
| 38 / 39 | (air) QCF×2 + P / K | Air SA1 / air SA3 |
| 24 | LP, LP, F, LK, HP | Shun Goku Satsu (Raging Demon) |
| 25 | D, D, D + PPP | Kongou Kokuretsuzan |
| 0 / 1 | F, F / B, B | Forward dash / back dash |

- **Strength:** each special step names a strength (light, medium, heavy or EX). That's the
  `[L]`/`[M]`/`[H]`/`[EX]` tag in the appendix. A "power level" of 8/9/10/11 picks the first, second,
  third or fourth button from the move's header (`renbanshot_conpaneshot`).
- **Normals:** the scripts also use his normals directly:
  - **Zugai Hasatsu** (f.MP overhead)
  - the **universal overhead** (MP+MK)
  - the **dive kick** (Tenmakujinkyaku, down+MK in the air)
  - **throws** (LP+LK, forward or back)
- **Command 34:** a "forward + kick" command he uses in one teleport script. Its first step can't be
  completed by any input (`check_20` does nothing), so it does nothing useful.

---

## 3. What he does on his own (Active)

At each decision point, he rolls 1 of 32 slots from the table for the current distance
(`Active_A..D_Unit_Data[14]`). These are the choices, grouped by type:

### Near (0–71)

| Choice | Levels 0–6 | Level 7 |
| --- | --- | --- |
| Close combo or blockstring straight away | 22% | 19% |
| Tatsumaki (only if you're **not crouching**) | 19% | 9% |
| Back dash, then jump-in dive kick into a close combo | 16% | 12% |
| Teleport (Ashura Senku), then Raging Demon attempt or Shoryuken | 12% | 6% |
| cr.MK, hit-confirm into Gou Hadouken (only if you're not crouching) | 9% | 6% |
| Single normal (MP, cr.LK, Zugai overhead…) | 9% | 6% |
| Back off / back dash | 6% | 6% |
| Walk forward and jump in, into a combo | – | 28% |

### Mid (72–127)

| Choice | Levels 0–6 | Level 7 |
| --- | --- | --- |
| Forward dash, then close combo | 34% | 9% |
| Jump-in dive kick (can cross up), then close combo | 12% | 28% |
| Long jump-in (low Zanku, j.HK, super-jump dive kick), then combo | 12% | 3% |
| Teleport | 9% | – |
| Tatsumaki (not vs crouching) | 9% | 6% |
| cr.MK into Gou Hadouken | 9% | 3% |
| Walk in, then close combo | – | 22% |
| Walk in, then jump-in combo | – | 9% |

### Far (128–191)

| Choice | Levels 0–6 | Level 7 |
| --- | --- | --- |
| Forward dash, then close combo | 28% | 25% |
| Jump-in picked from the reaction list | 19% | 12% |
| Teleport | 19% | 6% |
| Jump-in dive kick, then close combo | 16% | 16% |
| Walk in, then close combo | – | 12% |

### Very far (192+)

| Choice | Levels 0–6 | Level 7 |
| --- | --- | --- |
| Walk in, then long jump-in (low Zanku, j.HK, dive kick), then combo | 53% | 16% |
| Gou Hadouken | 19% | 25% |
| Teleport | 9% | 19% |
| Walk up and watch | 9% | 6% |
| Forward dash | 3% | 12% |

### His close combos and blockstrings

Most choices end in one of 16 short strings (patterns 76–91), each picked at 25% from a group of 4:

- **Low hit-confirms:**
  - cr.LK → cr.MK (with a Raging Demon attempt if he has full meter)
  - cr.LP → cr.LK → cr.MK
  - cr.LK → cr.MK → MK → Gou Hadouken
  - cr.LK → cr.LK → Tatsumaki → cr.LK → cr.MK
- **Mix-ups:**
  - cr.LK → HK
  - cr.LK → cr.MK → HP
  - MP target combo → cr.LK
  - universal overhead → cr.LK → cr.MK → HP
- **Long strings:** cr.LK → Tatsumaki → cr.LK → Tatsumaki → cr.LK → HK, and a version that starts
  with the overhead.
- **Throw set-ups (patterns 88–91):** LK or cr.LK, sometimes with one or two Tatsumakis, then walk right up
  and throw (forward or back). If he has full meter, he tries Raging Demon instead of the throw.

Most steps carry on as long as the previous one made contact, whether it hit, was blocked or was parried,
so they work as blockstrings. Making a step whiff, by walking back or jumping, ends the string. The steps
marked "continues only on hit" stop on a block or parry.

### Teleports

Ashura Senku shows up in many patterns (113–119 and the reaction list):

- After the teleport, if he has meter he tries **Shun Goku Satsu**. Otherwise he waits up to 5 frames for
  you to be within 80 and does a **light Gou Shoryuken**.
- Some versions teleport backward unless he has less than 128 or 256 px behind him. The others want 512 or
  768 px, so in practice they always go **forward**, through you.

<a id="supers"></a>
### Supers and Raging Demon

Before supers, many scripts check `SA_Term`. If his meter is ready, he jumps to the pattern for the super he
picked:

| Super | What he does |
| --- | --- |
| SA1 | Messatsu Gou Hadou (heavy) on the ground. From some patterns, a jumping air SA1: toward you up close, straight up at far range, backward at mid range |
| SA2 | Messatsu Gou Shoryu |
| SA3 | Messatsu Gou Rasen, sometimes jumping |

- **Raging Demon:** many scripts send every super choice to pattern 52 instead. That pattern
  only fires when the Raging Demon is available, which needs a full gauge (`permited_koa & 0x40`). It waits
  up to 5 frames for you to be within **208**, then inputs LP, LP, F, LK, HP.
- **Kongou Kokuretsuzan:** from level 6, one mid-range slot (pattern 150) tries it, with the same
  full-gauge condition. At level 7, pattern 149 does Kongou into a Tatsumaki hit-confirm.

---

## 4. How he reacts to you (Passive)

When you do something the CPU recognises, the passive tables (`Passive_A..D_Unit_Data_04[14]`) pick a
reaction based on the category of your action and the distance. The categories are internal codes, from
`ck_pass.c`, `com_pl.c` and `com_sub.c`. These are the ones that matter most against Akuma. The lists show
his choices at the top reaction level, with "dramatic" choices after the `|`.

| Code | Your action | Near | Mid | Far |
| --- | --- | --- | --- | --- |
| 0 | You're knocked down | Walk in and wait for you to get up, then a cross-up jump-in into a combo; Zugai overhead or cr.HK on wake-up; back dash; Ashura; a **taunt** if you're under half life and he's above three quarters | Wait for wake-up, then cr.HK / overhead; walk back and Gou Hadouken; jump-in combos | Wake-up Gou Hadouken / Shakunetsu timed by distance; dash in |
| 1 | You dash | cr.LK hit-confirm into Shoryuken; LK; cr.HK when you get close \| walk in, cr.MK → Gou Hadouken | Nothing, or walk in into a random hit-confirm \| walk in, cr.MK → Hadouken/Shoryuken | Nothing \| walk in, cr.MK → Hadouken |
| 2 | You're **stunned** | **Raging Demon if he has full meter** (7 of 8 normal entries); otherwise walk in, HP → Gou Shoryuken → super | Same as near | Dash in, wait for you to land, combos |
| 3 | You start a jump | Neutral jump air-to-air (j.MK/j.HK), neutral-jump air Tatsu, HK, cr.HK as you come down \| **Gou Shoryuken**, which becomes SA2/SA3 if he has meter; back jump j.HK | Neutral jump air Tatsu, Ashura, Shoryuken, air-to-air | Shoryuken / air-to-air mix, or nothing |
| 9 | You stand still | **Throw** (walk in, forward or back throw), **Zugai overhead**, wait for you to be on the ground and cr.LK hit-confirm, back-jump Zanku \| back dash Gou/Shakunetsu Hadouken, single HP/HK | Throws, back dash, neutral-jump Zanku, dash in and combo, jump-in combos | Gou Hadouken, Zanku, Tatsu → Shoryuken, dash and jump-in combos |
| 11 | You throw a projectile close by | Wait for your recovery and **MP target combo**; universal overhead strings; **Ashura through it**; throw \| cr.MK → Hadouken/super | **Ashura through it**; neutral-jump Zanku; jump-in j.HK combos | Jump-in combos |
| 13 | He has just parried you | MP target combo; HK; Raging Demon or HP → Tatsu → Shoryuken; Tatsu → Shoryuken \| MK → Tatsu → Shoryuken combos | Same as near | Nothing, or a fireball |
| 14 | He blocked your special, super or EX | **cr.MK → Gou Hadouken → super**; Tatsu → Shoryuken; Raging Demon if available \| Gou Shoryuken → SA2/SA3 | Same, plus Kongou Kokuretsuzan → Tatsu | Walk in and Gou Hadouken; Tatsu → Shoryuken; jump-ins |
| 15 | You jump in on him | Nothing; **Ashura away/under**; walk in and light Shoryuken \| **Gou Shoryuken → super** | Block, or Ashura \| Shoryuken, back-jump Zanku | Block, Ashura |
| 18 | You're in the air nearby | Walk in under you and Tatsumaki when you land; Gou Hadouken as you land \| Shoryuken | Dash in and punish your landing \| Gou Shoryuken | Light Gou Hadouken or Tatsumaki as you land |
| 23 | You do a rising attack (Shoryuken-type) | **Dash in, wait for you to land, then HP → Gou Shoryuken → super** \| same, starting with a walk-in | Same | Same, or Raging Demon after a dash |
| 25 | You walk forward close to him | Universal-overhead strings, walk in into a random hit-confirm | Jump-in combos | Nothing |
| 28 | You're recovering from an attack | Punish: hit-confirm strings, walk in and combo, Raging Demon | Same | Same, plus Gou Hadouken |
| 29 | You're crouching | Teleport, **Zugai overhead**, **throw**, Raging Demon / combo, universal overhead strings | Overhead, cross-up jump-ins | Nothing |

Notes:

- **Code 3 / 15 / 18 (anti-airs):** Gou Shoryuken mostly comes in the dramatic half of the table. When he
  has meter it usually turns into SA2 or SA3.
- **Code 11 (your projectile up close):** this is the reaction table. Fireballs coming at him from further
  away are handled by the projectile logic in [section 6](#6-projectiles).
- **Code 31 (not identified):** he waits for you to be within 128 and does a heavy Gou Shoryuken, or
  LP / cr.MK → Gou Hadouken. I haven't pinned down what triggers this code.

---

## 5. Parrying

Whenever he could parry one of your attacks, Akuma rolls a number from 0 to 31 (`Check_Flip_Term`). He
parries if the roll is below his `Flip_Data` value:

| Level | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Akuma | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 10 |
| Ryu (for comparison) | 0 | 0 | 0 | 1 | 2 | 3 | 3 | 4 |

**10 out of 32 (31%) at the top levels is the highest of any character.** Everyone else tops out at 3–7.
The roll is lowered (making a parry more likely) by:

- 1 if Akuma's life is below 49
- 2 if he's behind in rounds
- 1 if the timer is below 15

All together that can reach 14/32 (44%).

When the CPU is in boss mode (`EM_Rank`, set at the 10th match and later), every character uses a shared
lower row instead (2–7 out of 32).

After a successful parry, he counter-attacks if a second roll is under `Flip_Attack_Data`: **4/32 at level 0
up to 31/32 at level 7**. So at high level, a parried attack is almost always punished. The punish comes
from reaction code 13 above.

---

## 6. Projectiles

When your projectile comes at him, he first works out how many frames it will take to reach him
(`Compute_Hit_Time`):

- **Within 28 frames:** he tries to parry it if the parry roll passes, otherwise he blocks.
- **More than 28 frames away:** the reaction table `VS_Shell_Active_Data[14]` picks from these:

| Pattern | Action |
| --- | --- |
| 0 | Block |
| 2 | Jump toward you over it |
| 3 / 4 / 5 / 8 | Jump over it and jump-in (j.MK, j.MP or j.HK), then MP / cr.MK |
| 6 | Neutral jump j.HK |
| 7 | Gou Hadouken back |
| 9 | Parry it |
| 11 / 12 | Jump in with Tatsumaki (EX if he has meter) |

At level 0 he mostly just jumps toward you over a normal fireball (10 of 16 entries). At level 7 the most
common answers are jumping over into **j.MK → cr.MK** (5 of 16) and **jump-in Tatsumaki** (3 of 16), plus
a parry.

On the two hardest difficulty settings, when he's nearly dead (under 4 life on the hardest setting, under 2
on the next), he also has a small extra chance to parry any projectile.

---

## 7. Practical summary

- **Close range:** about a fifth of the time he goes straight into a hit-confirm or blockstring. A
  Tatsumaki or a cr.MK into Gou Hadouken follows only if you're standing. **Crouch-blocking makes him skip
  those** (the `only if opponent is NOT crouching` checks).
- **Crouching for too long:** this triggers his anti-crouch options: Zugai overhead, universal overhead,
  throws and teleports.
- **Standing still:** he throws, uses the overhead, or back-jumps into a Zanku.
- **Mid and far range:** he mostly dashes in or jumps in. At level 7 the jump-in **dive kick** becomes his
  favourite mid-range opener, and the dive kick in these patterns is allowed to cross up.
- **Anything rising (DPs and the like):** if he sees one, he dashes in and waits for you to land before
  punishing.
- **Jumping at him:** this mostly gets air-to-airs and the occasional Ashura. Shoryuken anti-airs are much
  more common when he has meter or someone is low on life.
- **Full gauge:** be very careful when you're stunned, when he parries you, when he blocks your special,
  and after his teleport. All of these can lead into **Shun Goku Satsu**, which he inputs from up to 208
  away.
- **Parrying:** he parries up to about a third of your attacks at high level, more when he's losing, and
  nearly always punishes the parry.

---

<a id="appendix-every-pattern"></a>
## Appendix: every pattern

These lists were generated from `active14.c`, `pass14.c` and `shell14.c`. `A n` is an action pattern, `P n`
a reaction pattern and `S n` a projectile pattern. "Distance" is between the two characters, "his height"
is Akuma's height above the ground, and "your height" is yours. "Random" percentages come from
`Com_Rnd_Select_Data`.

<details>
<summary>Action patterns (A 0–150)</summary>

```
A 0: stand and watch a random time
A 1: wait a random time
A 2: LP
A 3: MP
A 4: HP
A 5: LK
A 6: MK
A 7: HK
A 8: cr.LP
A 9: cr.MP
A 10: cr.HP
A 11: cr.LK
A 12: cr.MK
A 13: cr.HK
A 14: Gou Hadouken [L]
A 15: Gou Hadouken [M]
A 16: Gou Hadouken [H]
A 17: Gou Hadouken [L] -> Gou Shoryuken [L]
A 18: Gou Shoryuken [L]
A 19: Tatsumaki Zankukyaku [L]
A 20: Tatsumaki Zankukyaku [M]
A 21: Tatsumaki Zankukyaku [H]
A 22: Tatsumaki Zankukyaku [L] (continues if it connects) -> Gou Shoryuken [L]
A 23: Tatsumaki Zankukyaku [M] (continues if it connects) -> Gou Shoryuken [L]
A 24: back off to distance 127 (walk or jump)
A 25: back off to distance 191 (walk or jump)
A 26: (ignore reactions until done) -> walk in to distance 191 -> jump toward; Zanku Hadouken (air fireball) [L] once his height >=48 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> Tatsumaki Zankukyaku [L] (continues only on hit) -> wait 5 frames -> if super meter is ready: SA1->A 47, SA2->-, SA3->A 49 -> Gou Shoryuken [H]
A 27: Zugai Hasatsu (f.MP overhead)
A 28: walk in to distance 191 -> jump toward; Zanku Hadouken (air fireball) [L] once his height >=48 [if you jump too: j.HK at distance <=128] -> Zugai Hasatsu (f.MP overhead)
A 29: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> Gou Hadouken [H] (then counter-attack mode)
A 30: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
A 31: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> Gou Hadouken [H] (then counter-attack mode)
A 32: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
A 33: (ignore reactions until done) -> walk in to distance 191 -> jump toward; Zanku Hadouken (air fireball) [L] once his height >=48 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> Tatsumaki Zankukyaku [L] -> wait 3 frames -> Gou Shoryuken [L]
A 34: (ignore reactions until done) -> walk in to distance 191 -> jump toward; air Tatsumaki [H] once his height >=48 -> Gou Shoryuken [L]
A 35: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> Gou Hadouken [H]
A 36: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
A 37: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] -> wait 3 frames -> Gou Shoryuken [L]
A 38: only if opponent is NOT crouching (else A 0) -> Tatsumaki Zankukyaku [L]
A 39: only if opponent is NOT crouching (else A 0) -> Tatsumaki Zankukyaku [L]
A 40: only if opponent is NOT crouching (else A 0) -> Tatsumaki Zankukyaku [L]
A 41: only if opponent is NOT crouching (else A 0) -> Tatsumaki Zankukyaku [L] (continues only on hit) -> Gou Shoryuken [L]
A 42: only if opponent is NOT crouching (else A 0) -> cr.MK (continues only on hit) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
A 43: only if opponent is NOT crouching (else A 0) -> cr.MK (continues only on hit) -> Gou Hadouken [H]
A 44: random: P 113 6%, P 113 31%, P 114 31%, P 115 31%
A 45: random: P 116 6%, P 116 31%, P 117 31%, P 118 31%
A 46: random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
A 47: SA1 Messatsu Gou Hadou [H]
A 48: SA2 Messatsu Gou Shoryu [H]
A 49: SA3 Messatsu Gou Rasen [H]
A 50: by distance: near->A 55 mid->A 55 far->A 54 very far->A 53
A 51: jump toward; SA3 Messatsu Gou Rasen [H] once distance <=80 and his height <=64
A 52: only if Shun Goku Satsu is available (else P 124) -> wait until distance <=208 (gives up after 5 frames -> A 0) -> press LP -> wait 1 frames -> press LP -> wait 1 frames -> hold forward -> wait 1 frames -> press LK -> wait 1 frames -> press HP
A 53: jump toward; SA1 Messatsu Gou Hadou [L] once his height >=32
A 54: jump straight up; SA1 Messatsu Gou Hadou [L] once his height >=32
A 55: jump back; SA1 Messatsu Gou Hadou [L] once his height >=32
A 56: walk in to distance 191 -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 116 6%, P 116 31%, P 117 31%, P 118 31%
A 57: forward dash -> random: P 132 6%, P 132 31%, P 133 31%, P 134 31%
A 58: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
A 59: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
A 60: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
A 61: walk in to distance 191 -> (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
A 62: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
A 63: walk in to distance 191 -> jump toward; Zanku Hadouken (air fireball) [L] once his height >=52 [if you jump too: j.HK at distance <=128] -> cr.MK (continues if it connects) -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
A 64: walk in to distance 191 -> jump toward; Zanku Hadouken (air fireball) [L] once his height >=52 [if you jump too: j.HK at distance <=128] -> Zugai Hasatsu (f.MP overhead) (continues if it connects) -> cr.MK (continues if it connects) -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
A 65: walk in to distance 191 -> jump toward; Zanku Hadouken (air fireball) [L] once his height >=52 [if you jump too: j.HK at distance <=128] -> cr.LK (continues if it connects) -> cr.LK (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
A 66: walk in to distance 191 -> jump toward; Zanku Hadouken (air fireball) [L] once his height >=52 [if you jump too: j.HK at distance <=128] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
A 67: (ignore reactions until done) -> walk in to distance 191 -> jump toward; air Tatsumaki [H] once his height >=64 -> Gou Shoryuken [L]
A 68: forward dash
A 69: walk in to distance 191 -> random: P 178 25%, P 179 25%, P 180 25%, P 182 25%
A 70: walk in to distance 191 -> random: P 185 25%, P 187 25%, P 188 25%, P 189 25%
A 71: walk in to distance 191 -> random: P 181 25%, P 183 25%, P 191 25%, P 192 25%
A 72: random: P 178 25%, P 179 25%, P 180 25%, P 182 25%
A 73: random: P 185 25%, P 187 25%, P 188 25%, P 189 25%
A 74: random: P 181 25%, P 183 25%, P 191 25%, P 192 25%
A 75: walk in to distance 127
A 76: cr.LK (continues if it connects) -> cr.MK -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52
A 77: cr.LP (continues if it connects) -> cr.LK (continues if it connects) -> cr.MK
A 78: cr.LK (continues if it connects) -> cr.MK (continues if it connects) -> MK (continues if it connects) -> Gou Hadouken [H]
A 79: cr.LK (continues if it connects) -> cr.LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> cr.MK
A 80: cr.LK (continues if it connects) -> HK
A 81: cr.LK (continues if it connects) -> cr.MK (continues if it connects) -> HP
A 82: MP (target combo) -> cr.LK
A 83: cr.LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> HK
A 84: cr.LK (continues if it connects) -> universal overhead (MP+MK) (continues if it connects) -> cr.LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> HK
A 85: universal overhead (MP+MK) (continues if it connects) -> cr.LK (continues if it connects) -> cr.MK (continues if it connects) -> HP
A 86: cr.LP (continues if it connects) -> universal overhead (MP+MK) (continues if it connects) -> cr.LK (continues if it connects) -> cr.MK
A 87: universal overhead (MP+MK) (continues if it connects) -> cr.LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> cr.LK (continues if it connects) -> hold down-back -> wait 2 frames -> cr.HK
A 88: cr.LK (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> walk in to distance 16 -> forward throw
A 89: LK (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> walk in to distance 16 -> back throw
A 90: LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> walk in to distance 16 -> forward throw
A 91: LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> Tatsumaki Zankukyaku [L] -> cr.LK (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> walk in to distance 16 -> back throw
A 92: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 93: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
A 94: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
A 95: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
A 96: forward dash -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 97: forward dash -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
A 98: forward dash -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
A 99: forward dash -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
A 100: walk in to distance 191 -> jump toward; air Tatsumaki [H] once distance <=160 and his height >=80 [if you jump too: j.HK at distance <=128] -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 101: walk in to distance 191 -> jump toward; air Tatsumaki [H] once distance <=160 and his height >=80 [if you jump too: j.HK at distance <=128] -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
A 102: walk in to distance 191 -> jump toward; air Tatsumaki [H] once distance <=160 and his height >=80 [if you jump too: j.HK at distance <=128] -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
A 103: walk in to distance 191 -> jump toward; air Tatsumaki [H] once distance <=160 and his height >=80 [if you jump too: j.HK at distance <=128] -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
A 104: random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 105: random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
A 106: random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
A 107: random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
A 108: back dash -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 109: back dash -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
A 110: back dash -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
A 111: back dash -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
A 112: back dash
A 113: cmd34 (F+K) [L] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
A 114: if less than 768 px of room behind -> A 118 -> Ashura Senku (back) [H] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
A 115: if less than 128 px of room behind -> A 118 -> Ashura Senku (back) [H] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
A 116: if less than 512 px of room behind -> A 119 -> Ashura Senku (back) [L] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
A 117: if less than 256 px of room behind -> A 119 -> Ashura Senku (back) [L] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
A 118: Ashura Senku (forward) [H] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
A 119: Ashura Senku (forward) [L] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
A 120: walk in to distance 191 -> stand and watch 2 frames
A 121: walk in to distance 114 -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 122: walk in to distance 114 -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
A 123: walk in to distance 114 -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
A 124: walk in to distance 114 -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
A 125: random: A 33 25%, A 63 25%, A 64 25%, A 65 25%
A 126: random: A 35 25%, A 36 25%, A 37 25%, A 61 25%
A 127: random: A 34 25%, A 67 25%, A 60 25%, A 62 25%
A 128: walk in to distance 191 -> stand and watch 2 frames -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 129: walk in to distance 114 -> stand and watch 2 frames -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 130: random: A 113 25%, A 114 25%, A 113 25%, A 114 25%
A 131: random: A 26 25%, A 28 25%, A 29 25%, A 30 25%
A 132: walk in to distance 120 -> cr.MK
A 133: walk in to distance 134 -> cr.HK
A 134: walk in to distance 119 -> MK
A 135: walk in to distance 132 -> HK
A 136: random: A 132 25%, A 133 25%, A 134 25%, A 135 25%
A 137: MK (continues if it connects) -> cr.MK (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] (continues only on hit) -> wait 5 frames -> if super meter is ready: SA1->A 47, SA2->-, SA3->A 49 -> Gou Shoryuken [H]
A 138: cr.MK (continues if it connects) -> cr.MK (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] (continues only on hit) -> wait 5 frames -> if super meter is ready: SA1->A 47, SA2->-, SA3->A 49 -> Gou Shoryuken [H]
A 139: MK (continues if it connects) -> cr.MK (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [M] (continues only on hit) -> wait 3 frames -> Gou Shoryuken [M]
A 140: MK (continues if it connects) -> MK (continues if it connects) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [M] (continues only on hit) -> wait 3 frames -> Gou Shoryuken [M]
A 141: random: A 137 25%, A 138 25%, A 139 25%, A 140 25%
A 142: walk in to distance 87 -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 137 25%, A 138 25%, A 139 25%, A 140 25%
A 143: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 137 25%, A 138 25%, A 139 25%, A 140 25%
A 144: random: A 38 25%, A 39 25%, A 40 25%, A 41 25%
A 145: walk in to distance 87 -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
A 146: walk in to distance 87 -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
A 147: walk in to distance 87 -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
A 148: walk in to distance 87 -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
A 149: Kongou Kokuretsuzan [H] -> Tatsumaki Zankukyaku [M] (continues only on hit)
A 150: only if Shun Goku Satsu is available (else P 124) -> wait until distance <=208 (gives up after 5 frames -> A 0) -> Kongou Kokuretsuzan [H]
```

</details>

<details>
<summary>Reaction patterns (P 0–252)</summary>

```
P 0: a normal picked for the situation (target combo)
P 1: stand and watch a random time
P 2: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> end) -> LP
P 3: walk in to distance 71 -> wait until your height <=36 (gives up after 30 frames -> end) -> HP
P 4: walk in to distance 71 -> wait until your height <=32 (gives up after 30 frames -> end) -> HK
P 5: walk in to distance 16 -> wait until your height <=8 (gives up after 60 frames -> end) -> forward throw
P 6: walk in to distance 16 -> wait until your height <=8 (gives up after 60 frames -> end) -> back throw
P 7: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> MP (target combo)
P 8: walk in to distance 71 -> Gou Shoryuken [L]
P 9: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> Gou Hadouken [H]
P 10: walk in to distance 63 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> Gou Shoryuken [H]
P 11: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] -> wait 3 frames -> Gou Shoryuken [L]
P 12: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> Tatsumaki Zankukyaku [H]
P 13: wait until your height <=256 (gives up after 60 frames -> P 124) -> Shakunetsu Hadouken [H]
P 14: wait until your height <=56 (gives up after 60 frames -> P 124) -> Gou Hadouken [L]
P 15: walk in to distance 127 -> wait until your height <=48 (gives up after 60 frames -> P 124) -> cr.HK
P 16: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> Tatsumaki Zankukyaku [L] (continues if it connects) -> (ignore reactions until done) -> wait 3 frames -> Gou Shoryuken [H] (then counter-attack mode) -> wait 3 frames -> if super meter is ready: SA1->-, SA2->-, SA3->A 49
P 17: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> Gou Shoryuken [H] (then counter-attack mode) -> wait 4 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 18: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> Tatsumaki Zankukyaku [H]
P 19: walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> end) -> cr.HP (then counter-attack mode) -> Gou Shoryuken [H]
P 20: walk in to distance 127 -> wait until your height <=16 (gives up after 30 frames -> end) -> HP
P 21: walk in to distance 191 -> wait until your height <=128 (gives up after 60 frames -> P 124) -> Gou Hadouken [H]
P 22: wait until your height <=80 (gives up after 60 frames -> P 124) -> Tatsumaki Zankukyaku [H]
P 23: (ignore reactions until done) -> jump toward; air Tatsumaki [H] once his height >=48 -> Gou Shoryuken [L]
P 24: forward dash -> walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> Gou Shoryuken [L]
P 25: forward dash -> walk in to distance 16 -> wait until your height <=8 (gives up after 60 frames -> end) -> forward throw
P 26: forward dash -> walk in to distance 16 -> wait until your height <=8 (gives up after 60 frames -> end) -> back throw
P 27: forward dash -> walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> MP (target combo)
P 28: forward dash -> walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> Gou Shoryuken [H] (then counter-attack mode) -> wait 4 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 29: forward dash -> walk in to distance 71 -> wait until your height <=16 (gives up after 60 frames -> end) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 30: forward dash -> walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> cr.MK (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] (continues only on hit) -> wait 3 frames -> Gou Shoryuken [L]
P 31: forward dash -> walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> cr.MK (then counter-attack mode) -> Tatsumaki Zankukyaku [H]
P 32: forward dash -> walk in to distance 127 -> wait until your height <=48 (gives up after 60 frames -> P 124) -> cr.HK
P 33: forward dash -> walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> Gou Hadouken [H]
P 34: forward dash -> walk in to distance 63 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> Gou Shoryuken [H]
P 35: forward dash -> walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> HP (then counter-attack mode) -> Tatsumaki Zankukyaku [L] (continues only on hit) -> (ignore reactions until done) -> wait 3 frames -> Gou Shoryuken [H] (then counter-attack mode) -> wait 3 frames -> if super meter is ready: SA1->-, SA2->-, SA3->A 49
P 36: if super meter is ready: SA1->-, SA2->A 48, SA3->A 49 -> Gou Shoryuken [L]
P 37: wait until distance <=96 and your height <=96 (gives up after 60 frames -> P 53) -> if super meter is ready: SA1->-, SA2->A 48, SA3->A 49 -> Gou Shoryuken [L]
P 38: if super meter is ready: SA1->-, SA2->A 48, SA3->A 49 -> Gou Shoryuken [L]
P 39: wait until your height <=80 (gives up after 60 frames -> P 53) -> if super meter is ready: SA1->-, SA2->A 48, SA3->A 49 -> Gou Shoryuken [L]
P 40: wait 3 frames -> wait until distance <=64 and your height <=56 (gives up after 60 frames -> P 53) -> cr.HK
P 41: forward dash -> walk in to distance 127 -> wait until your height <=48 (gives up after 60 frames -> P 53) -> cr.HK
P 42: back dash -> Gou Hadouken [H]
P 43: jump straight up; j.MK once distance <=96 and his height >=56 [if you jump too: j.MK at distance <=96]
P 44: jump straight up; j.HK once distance <=96 and his height >=56 [if you jump too: j.HK at distance <=96]
P 45: (ignore reactions until done) -> jump straight up; air Tatsumaki [H] once his height >=16 -> if super meter is ready: SA1->-, SA2->-, SA3->A 49 -> Gou Shoryuken [H]
P 46: walk in to distance 127 -> wait until your height <=16 (gives up after 60 frames -> end) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 47: walk in to distance 71 -> wait until your height <=16 (gives up after 60 frames -> end) -> cr.MK (then counter-attack mode) -> Gou Shoryuken [H] (then counter-attack mode) -> wait 3 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 48: jump back; j.HK once distance <=128 and his height >=32 [if you jump too: j.HK at distance <=128]
P 49: wait until distance <=80 and your height <=80 (gives up after 60 frames -> P -1) -> HK
P 50: wait until distance <=64 and your height <=16 (gives up after 60 frames -> end) -> LK
P 51: wait until distance <=80 and your height <=64 (gives up after 30 frames -> end) -> cr.LK (continues only on hit) -> Gou Shoryuken [L]
P 52: walk in to distance 71 -> cr.HK (continues only on hit)
P 53: block (0)
P 54: wait for opponent to get up
P 55: wait for opponent to get up
P 56: wait for opponent to get up -> cr.HK
P 57: wait for opponent to get up -> Zugai Hasatsu (f.MP overhead)
P 58: wait for opponent to get up -> cr.HK
P 59: wait for opponent to get up -> Zugai Hasatsu (f.MP overhead)
P 60: back off to distance 191 (walk or jump) -> wait for opponent to get up
P 61: back off to distance 191 (walk or jump) -> wait for opponent to get up
P 62: back off to distance 191 (walk or jump) -> wait for opponent to get up -> Gou Hadouken [H]
P 63: only if opponent's life < 80 and Akuma's >= 120 (else P 55) -> taunt -> go to P 57
P 64: back off to distance 127 (walk or jump) -> wait for opponent to get up
P 65: back off to distance 127 (walk or jump) -> wait for opponent to get up
P 66: back off to distance 127 (walk or jump) -> wait for opponent to get up -> Gou Hadouken [H]
P 67: wait for opponent to get up -> cr.MK
P 68: wait for opponent to get up -> if less than 224 px of room behind -> P 69 -> jump toward; j.MK once distance <=88 and his height <=48 [if you jump too: j.HK at distance <=128] -> Gou Shoryuken [H]
P 69: wait for opponent to get up -> wait 20/15/5/1 frames (near/mid/far/very far) -> Gou Hadouken [M]
P 70: wait for opponent to get up -> wait 20/15/5/1 frames (near/mid/far/very far) -> Gou Hadouken [H]
P 71: walk in to distance 127 -> wait for opponent to get up -> cr.HK
P 72: walk in to distance 127 -> wait for opponent to get up -> Zugai Hasatsu (f.MP overhead)
P 73: wait for opponent to get up -> wait 10/7/3/1 frames (near/mid/far/very far) -> Shakunetsu Hadouken [L]
P 74: wait for opponent to get up -> wait 10/7/3/1 frames (near/mid/far/very far) -> Shakunetsu Hadouken [M]
P 75: wait for opponent to get up -> wait 10/7/3/1 frames (near/mid/far/very far) -> Shakunetsu Hadouken [H]
P 76: wait for opponent to get up -> wait 15/10/5/1 frames (near/mid/far/very far) -> Tatsumaki Zankukyaku [H]
P 77: wait for opponent to get up -> if less than 224 px of room behind -> P 69 -> jump toward; j.MK once distance <=88 and his height <=32 [if you jump too: j.HK at distance <=128] -> Tatsumaki Zankukyaku [H]
P 78: only if opponent's life < 80 and Akuma's >= 120 (else P 55) -> taunt -> go to P 70
P 79: forward dash
P 80: forward dash -> forward dash
P 81: if less than 768 px of room behind -> P 227 -> Ashura Senku (back) [H] -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 82: if less than 128 px of room behind -> P 227 -> Ashura Senku (back) [H] -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 83: if less than 512 px of room behind -> P 228 -> Ashura Senku (back) [L] -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 84: if less than 256 px of room behind -> P 228 -> Ashura Senku (back) [L] -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 85: cr.MK (continues only on hit) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 86: cr.MK (continues if it connects) -> cr.MK
P 87: cr.LK (continues only on hit) -> Tatsumaki Zankukyaku [H]
P 88: cr.LK (continues only on hit) -> Gou Shoryuken [L]
P 89: cr.LK (continues only on hit) -> Gou Hadouken [H]
P 90: cr.MK (continues only on hit) -> Tatsumaki Zankukyaku [H]
P 91: cr.MK (then counter-attack mode) -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> Gou Shoryuken [L]
P 92: cr.MK (then counter-attack mode) -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> Gou Shoryuken [H]
P 93: cr.MK (then counter-attack mode) -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> Gou Hadouken [H]
P 94: universal overhead (MP+MK) (continues if it connects) -> cr.LK (continues if it connects) -> cr.MK (continues if it connects) -> Gou Hadouken [L]
P 95: universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 96: universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Shoryuken [H] (then counter-attack mode) -> wait 3 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 97: back off to distance 191 (jump) -> random: P 21 25%, P 29 25%, P 30 25%, P 31 25%
P 98: MK (continues if it connects) -> cr.MK
P 99: MK (continues if it connects) -> MK
P 100: cr.LK (continues if it connects) -> cr.MK
P 101: cr.LK (continues if it connects) -> cr.MK (stops if blocked/parried) -> cr.HK
P 102: cr.LK (continues if it connects) -> cr.LK (stops if blocked/parried) -> Gou Hadouken [H]
P 103: cr.LK (continues if it connects) -> cr.MK (stops if blocked/parried) -> Gou Hadouken [H]
P 104: cr.LK (continues if it connects) -> cr.MK (stops if blocked/parried) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 105: back dash -> Gou Hadouken [H]
P 106: back dash -> Shakunetsu Hadouken [L]
P 107: wait a random time
P 108: walk in to distance 71 -> MP
P 109: walk in to distance 71 -> HP
P 110: walk in to distance 71 -> HK
P 111: walk in to distance 71 -> cr.HK
P 112: walk in to distance 71 -> MP (then counter-attack mode) -> Gou Shoryuken [H]
P 113: jump straight up; Zanku Hadouken (air fireball) [L] once his height >=32 [if you jump too: j.HK at distance <=128]
P 114: jump straight up; Zanku Hadouken (air fireball) [M] once his height >=32 [if you jump too: j.HK at distance <=128]
P 115: jump straight up; Zanku Hadouken (air fireball) [H] once his height >=32 [if you jump too: j.HK at distance <=128]
P 116: jump toward; Zanku Hadouken (air fireball) [L] once his height >=32 [if you jump too: j.HK at distance <=128]
P 117: jump toward; Zanku Hadouken (air fireball) [M] once his height >=32 [if you jump too: j.HK at distance <=128]
P 118: jump toward; Zanku Hadouken (air fireball) [H] once his height >=32 [if you jump too: j.HK at distance <=128]
P 119: jump back; Zanku Hadouken (air fireball) [L] once his height >=32 [if you jump too: j.HK at distance <=128]
P 120: jump back; Zanku Hadouken (air fireball) [M] once his height >=32 [if you jump too: j.HK at distance <=128]
P 121: jump back; Zanku Hadouken (air fireball) [H] once his height >=32 [if you jump too: j.HK at distance <=128]
P 122: random: P 113 6%, P 113 31%, P 114 31%, P 115 31%
P 123: random: P 116 6%, P 116 31%, P 117 31%, P 118 31%
P 124: random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 125: wait until your height <=8 (gives up after 60 frames -> P 1) -> random: P 87 6%, P 87 31%, P 88 31%, P 89 31%
P 126: wait until your height <=8 (gives up after 60 frames -> P 1) -> random: P 90 6%, P 91 31%, P 92 31%, P 93 31%
P 127: walk in to distance 71 -> random: P 87 6%, P 87 31%, P 88 31%, P 89 31%
P 128: walk in to distance 71 -> random: P 90 6%, P 91 31%, P 92 31%, P 93 31%
P 129: walk in to distance 16 -> forward throw
P 130: walk in to distance 16 -> back throw
P 131: random: P 129 25%, P 129 25%, P 130 25%, P 130 25%
P 132: Gou Hadouken [L]
P 133: Gou Hadouken [M]
P 134: Gou Hadouken [H]
P 135: Shakunetsu Hadouken [L]
P 136: Shakunetsu Hadouken [M]
P 137: Shakunetsu Hadouken [H]
P 138: random: P 132 6%, P 132 31%, P 133 31%, P 134 31%
P 139: random: P 135 6%, P 135 31%, P 136 31%, P 137 31%
P 140: Tatsumaki Zankukyaku [L] (continues if it connects) -> Gou Shoryuken [L]
P 141: Tatsumaki Zankukyaku [M] (continues if it connects) -> Gou Shoryuken [L]
P 142: Tatsumaki Zankukyaku [H] (continues if it connects) -> Gou Shoryuken [L]
P 143: wait until your height <=8 (gives up after 60 frames -> P 1) -> random: P 140 6%, P 140 31%, P 141 31%, P 142 31%
P 144: Gou Shoryuken [L]
P 145: Gou Shoryuken [L] -> random: P 144 25%
P 146: jump toward; Zanku Hadouken (air fireball) [L] once his height >=52 -> if super meter is ready: SA1->A 47, SA2->-, SA3->-
P 147: only if Shun Goku Satsu is available (else P 118) -> press LP -> wait 1 frames -> press LP -> wait 1 frames -> hold forward -> wait 1 frames -> press LK -> wait 1 frames -> press HP
P 148: only if super ready (else P 118) -> jump toward; SA1 Messatsu Gou Hadou [H] once his height >=32
P 149: random: P 146 6%, P 146 31%, P 147 31%, P 148 31%
P 150: universal overhead (MP+MK) (continues if it connects) -> random: P 129 25%, P 130 25%, P 129 25%, P 130 25%
P 151: jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once distance >=8 and his height >=73 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (continues only on hit) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 52, SA3->A 52
P 152: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (continues only on hit) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 52, SA3->A 52
P 153: Zugai Hasatsu (f.MP overhead)
P 154: wait until your height <=8 (gives up after 60 frames -> P 1) -> MP (target combo)
P 155: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H]
P 156: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (continues only on hit) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 52, SA3->A 52
P 157: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
P 158: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (continues only on hit) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 52, SA3->A 52 -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 159: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
P 160: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 161: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
P 162: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 163: back dash
P 164: back dash -> wait for opponent to get up -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 113 6%, P 113 31%, P 114 31%, P 115 31%
P 165: walk in to distance 191 -> wait for opponent to get up -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 116 6%, P 116 31%, P 117 31%, P 118 31%
P 166: forward dash -> random: P 132 6%, P 132 31%, P 133 31%, P 134 31%
P 167: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
P 168: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (continues only on hit) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 52, SA3->A 52
P 169: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 170: walk in to distance 191 -> (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 171: walk in to distance 191 -> jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 172: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
P 173: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 174: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 175: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 176: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 177: random: A 63 25%, A 64 25%, A 65 25%, A 66 25%
P 178: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
P 179: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (continues only on hit) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 52, SA3->A 52
P 180: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 181: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues if it connects) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 182: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> universal overhead (MP+MK) (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 183: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues if it connects) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 184: walk in to distance 191 -> Gou Hadouken [H] (continues only on hit) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->-, SA3->-
P 185: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (continues only on hit) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 52, SA3->A 52
P 186: walk in to distance 191 -> random: P 178 25%, P 179 25%, P 180 25%, P 182 25%
P 187: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
P 188: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 189: jump toward; j.HK once distance <=88 and his height <=56 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 190: walk in to distance 191 -> random: P 185 25%, P 187 25%, P 188 25%, P 189 25%
P 191: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (continues only on hit) -> if super meter is ready: SA1->A 47, SA2->A 52, SA3->A 52 -> random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 192: (allow crossing over) -> super jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=97 [if you jump too: j.HK at distance <=128] -> cr.MK (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] (continues only on hit) -> wait 3 frames -> Gou Shoryuken [L]
P 193: walk in to distance 191 -> random: P 181 25%, P 183 25%, P 191 25%, P 192 25%
P 194: random: P 178 25%, P 179 25%, P 180 25%, P 182 25%
P 195: random: P 185 25%, P 187 25%, P 188 25%, P 189 25%
P 196: random: P 181 25%, P 183 25%, P 191 25%, P 192 25%
P 197: forward dash -> walk in to distance 71 -> wait until your height <=8 (gives up after 60 frames -> P 124) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> HP (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] -> wait 3 frames -> Gou Shoryuken [L]
P 198: HK
P 199: wait for your attack to finish -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> cr.MK (then counter-attack mode) -> Gou Hadouken [H]
P 200: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
P 201: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
P 202: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
P 203: (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
P 204: forward dash -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
P 205: forward dash -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
P 206: forward dash -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
P 207: forward dash -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
P 208: walk in to distance 181 -> jump toward; air Tatsumaki [H] once distance <=160 and his height >=80 [if you jump too: j.HK at distance <=128] -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
P 209: walk in to distance 191 -> jump toward; air Tatsumaki [H] once distance <=160 and his height >=80 [if you jump too: j.HK at distance <=128] -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
P 210: walk in to distance 173 -> jump toward; air Tatsumaki [H] once distance <=160 and his height >=80 [if you jump too: j.HK at distance <=128] -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
P 211: walk in to distance 191 -> jump toward; air Tatsumaki [H] once distance <=160 and his height >=80 [if you jump too: j.HK at distance <=128] -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
P 212: if super meter is ready: SA1->A 50, SA2->-, SA3->- -> random: P 113 6%, P 113 31%, P 114 31%, P 115 31%
P 213: wait until your height <=8 (gives up after 60 frames -> P 1) -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
P 214: wait until your height <=8 (gives up after 60 frames -> P 1) -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
P 215: wait until your height <=8 (gives up after 60 frames -> P 1) -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
P 216: wait until your height <=8 (gives up after 60 frames -> P 1) -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
P 217: back dash -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
P 218: back dash -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
P 219: back dash -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
P 220: back dash -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
P 221: if less than 768 px of room behind -> P 225 -> Ashura Senku (back) [H] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 222: if less than 128 px of room behind -> P 225 -> Ashura Senku (back) [H] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 223: if less than 512 px of room behind -> P 226 -> Ashura Senku (back) [L] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 224: if less than 256 px of room behind -> P 226 -> Ashura Senku (back) [L] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 225: Ashura Senku (forward) [H] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 226: Ashura Senku (forward) [L] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 227: Ashura Senku (forward) [H] -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 228: Ashura Senku (forward) [L] -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 229: cmd34 (F+K) [L] -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> wait until distance <=80 (gives up after 5 frames -> P 1) -> Gou Shoryuken [L]
P 230: wait until your height <=8 (gives up after 60 frames -> P 1) -> only if Shun Goku Satsu is available (else P 17) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> HP (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] -> wait 3 frames -> Gou Shoryuken [L]
P 231: forward dash -> only if Shun Goku Satsu is available (else P 17) -> if super meter is ready: SA1->A 52, SA2->A 52, SA3->A 52 -> HP (then counter-attack mode) -> (ignore reactions until done) -> Tatsumaki Zankukyaku [L] -> wait 3 frames -> Gou Shoryuken [L]
P 232: walk in to distance 191 -> stand and watch 2 frames
P 233: walk in to distance 191 -> stand and watch 2 frames
P 234: wait for your attack to finish -> wait until your height <=8 (gives up after 60 frames -> P 1) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H]
P 235: wait until distance <=128 (gives up after 60 frames -> P 1) -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49 -> Gou Shoryuken [L]
P 236: wait until distance <=152 (gives up after 60 frames -> P 1) -> wait until your height <=8 (gives up after 1 frame -> P 235) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode) -> wait 1 frames -> if super meter is ready: SA1->A 47, SA2->A 48, SA3->A 49
P 237: wait until distance <=128 (gives up after 60 frames -> P 1) -> Gou Shoryuken [H]
P 238: LP (continues if it connects) -> cr.MK (then counter-attack mode) -> Gou Hadouken [H]
P 239: jump toward
P 240: wait for opponent to get up -> random: A 132 25%, A 133 25%, A 134 25%, A 135 25%
P 241: walk in to distance 87 -> wait for opponent to get up -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 137 25%, A 138 25%, A 139 25%, A 140 25%
P 242: wait for opponent to get up -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 137 25%, A 138 25%, A 139 25%, A 140 25%
P 243: random: A 38 25%, A 39 25%, A 40 25%, A 41 25%
P 244: walk in to distance 87 -> wait for opponent to get up -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 76 25%, A 77 25%, A 78 25%, A 79 25%
P 245: walk in to distance 87 -> wait for opponent to get up -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 80 25%, A 81 25%, A 82 25%, A 83 25%
P 246: walk in to distance 87 -> wait for opponent to get up -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 84 25%, A 85 25%, A 86 25%, A 87 25%
P 247: walk in to distance 87 -> wait for opponent to get up -> (allow crossing over) -> jump toward; dive kick (Tenmakujinkyaku, j.down+MK) once his height >=73 [if you jump too: j.HP at distance <=128] -> random: A 88 25%, A 89 25%, A 90 25%, A 91 25%
P 248: random: A 137 25%, A 138 25%, A 139 25%, A 140 25%
P 249: cr.HK (then counter-attack mode) -> Gou Hadouken [H] (then counter-attack mode)
P 250: random: P 119 6%, P 119 31%, P 120 31%, P 121 31%
P 251: Kongou Kokuretsuzan [H] -> Tatsumaki Zankukyaku [M] (continues only on hit)
P 252: only if Shun Goku Satsu is available (else P 124) -> wait until distance <=208 (gives up after 5 frames -> A 0) -> Kongou Kokuretsuzan [H]
```

</details>

<details>
<summary>Projectile patterns (S 0–12)</summary>

```
S 0: (nothing)
S 1: wait for the fireball to get close -> jump straight up
S 2: wait for the fireball to get close -> jump toward
S 3: wait for the fireball to get close -> jump toward; j.MK once distance <=88 and his height <=48 [if you jump too: j.MK at distance <=80] -> MP (continues only on hit) -> cr.MK
S 4: wait for the fireball to get close -> jump toward; j.MK once distance <=88 and his height <=48 [if you jump too: j.MP at distance <=80] -> cr.MK
S 5: wait for the fireball to get close -> jump toward; j.MK once distance <=88 and his height <=64 [if you jump too: j.HK at distance <=80] -> cr.MK
S 6: wait for the fireball to get close -> jump straight up; j.HK once his height >=48
S 7: Gou Hadouken [L]
S 8: wait for the fireball to get close -> jump toward; j.MP once distance <=88 and his height <=48 [if you jump too: j.MK at distance <=80] -> cr.MK
S 9: wait for the fireball to get close -> parry it
S 10: (nothing)
S 11: jump toward; Tatsumaki Zankukyaku [H] once his height >=48
S 12: jump toward; Tatsumaki Zankukyaku [EX if meter, else H] once his height >=48
```

</details>
