#ifndef EFF99_H
#define EFF99_H

#include "sf33rd/Source/Game/engine/workuser.h"
#include "structs.h"
#include "types.h"

/// Stage/handicap labels. Row 3 is the stage location, row 4 the stage owner, indexed by `Stage`.
extern const char* Letter_Data_99[5][VS_STAGE_RANDOM + 1];

void effect_99_move(WORK_Other_CONN* ewk);
s32 effect_99_init(s16 id, s16 type, s16 char_offset, s16 letter_index, s16 pos_index, s16 master_player);

#endif
