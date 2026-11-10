/**
 * @file training_display.h
 * Port-only training mode displays: frame advantage, red parry timing, stun timer, guard direction, juggle state
 * and charge meter.
 */

#ifndef TRAINING_DISPLAY_H
#define TRAINING_DISPLAY_H

#include "structs.h"
#include "types.h"

#include <stdbool.h>

typedef enum TrainingDisplayKind {
    TRAINING_DISPLAY_FRAME_ADVANTAGE,
    TRAINING_DISPLAY_RED_PARRY_TIMING,
    TRAINING_DISPLAY_STUN_TIMER,
    TRAINING_DISPLAY_GUARD_DIRECTION,
    TRAINING_DISPLAY_JUGGLE,
    TRAINING_DISPLAY_CHARGE,
    TRAINING_DISPLAY_COUNT,
} TrainingDisplayKind;

const char* TrainingDisplay_GetName(TrainingDisplayKind kind);
bool TrainingDisplay_IsEnabled(TrainingDisplayKind kind);
void TrainingDisplay_Toggle(TrainingDisplayKind kind);

/// Saves the on/off settings. Call when leaving the settings menu.
void TrainingDisplay_SaveSettings();

/// Clears everything being tracked, at the start of a round.
void TrainingDisplay_Reset();

/// Call once per game frame, after the players have moved.
void TrainingDisplay_Update();

void TrainingDisplay_Render();

/// A player has just started guarding a hit.
void TrainingDisplay_NoteGuard(PLW* wk);

/// A hit was guarded during blockstun while a parry input was live but too old to count as a red parry.
void TrainingDisplay_NoteRedParryEarly(PLW* wk, s16 frames);

/// A hit was guarded during blockstun with no parry input live, so a red parry input after it is late.
void TrainingDisplay_NoteRedParryChance(PLW* wk);

#endif
