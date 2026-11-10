/**
 * @file training_display.c
 * Port-only training mode displays, modelled on effie's 3rd training lua.
 *
 * Everything here only reads game state; nothing feeds back into the simulation.
 */

#include "sf33rd/Source/Game/ui/training_display.h"
#include "common.h"
#include "port/paths.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/dc_ghost.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/glyph_renderer.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"

#include <SDL3/SDL.h>

#define SETTINGS_FILE "training_displays.txt"

#define ADVANTAGE_SHOW_FRAMES 150
#define RED_PARRY_SHOW_FRAMES 90
#define GUARD_SHOW_FRAMES 60
#define RED_PARRY_LATE_WINDOW 10

#define LABEL_HEIGHT 116 // Height above a player's feet that the labels start at
#define LINE_HEIGHT 9

#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_GREEN 0xFF70FF70
#define COLOR_RED 0xFFFF6060
#define COLOR_BLUE 0xFF70B0FF
#define COLOR_YELLOW 0xFFFFFF60

#define LEVER_UP 1
#define LEVER_DOWN 2
#define LEVER_LEFT 4
#define LEVER_RIGHT 8

typedef struct TimedText {
    char text[16];
    u32 color;
    s16 timer;
} TimedText;

typedef struct PlayerDisplay {
    TimedText advantage;
    TimedText red_parry;
    s16 guard_timer;
    u16 guard_lever;

    /// Frame of the guarded hit a late red parry input is measured from, or -1
    s32 red_late_from;
    s16 prev_parry_flags[3];

    /// Frame this player first became able to act in the current exchange, or -1
    s32 first_free;
    bool was_hit;
} PlayerDisplay;

static const char* const display_names[TRAINING_DISPLAY_COUNT] = {
    [TRAINING_DISPLAY_FRAME_ADVANTAGE] = "FRAME ADVANTAGE",
    [TRAINING_DISPLAY_RED_PARRY_TIMING] = "RED PARRY TIMING",
    [TRAINING_DISPLAY_STUN_TIMER] = "STUN TIMER",
    [TRAINING_DISPLAY_GUARD_DIRECTION] = "GUARD DIRECTION",
    [TRAINING_DISPLAY_JUGGLE] = "JUGGLE STATE",
    [TRAINING_DISPLAY_CHARGE] = "CHARGE METER",
};

static bool enabled[TRAINING_DISPLAY_COUNT];
static bool settings_loaded;

static PlayerDisplay players[2];
static s32 frame;
static bool exchange_active;
static s16 exchange_defender;

static bool is_active() {
    return Mode_Type == MODE_NORMAL_TRAINING;
}

static char* settings_path() {
    char* path = NULL;
    SDL_asprintf(&path, "%s%s", Paths_GetPrefPath(), SETTINGS_FILE);
    return path;
}

static void load_settings() {
    char* path;
    char* data;
    size_t size = 0;

    if (settings_loaded) {
        return;
    }

    settings_loaded = true;
    path = settings_path();
    data = SDL_LoadFile(path, &size);
    SDL_free(path);

    if (data == NULL) {
        return;
    }

    for (size_t i = 0; i < size && i < TRAINING_DISPLAY_COUNT; i++) {
        enabled[i] = data[i] == '1';
    }

    SDL_free(data);
}

void TrainingDisplay_SaveSettings() {
    char data[TRAINING_DISPLAY_COUNT + 1];
    char* path;

    for (int i = 0; i < TRAINING_DISPLAY_COUNT; i++) {
        data[i] = enabled[i] ? '1' : '0';
    }

    data[TRAINING_DISPLAY_COUNT] = '\n';
    path = settings_path();
    SDL_SaveFile(path, data, sizeof(data));
    SDL_free(path);
}

const char* TrainingDisplay_GetName(TrainingDisplayKind kind) {
    return display_names[kind];
}

bool TrainingDisplay_IsEnabled(TrainingDisplayKind kind) {
    load_settings();
    return enabled[kind];
}

void TrainingDisplay_Toggle(TrainingDisplayKind kind) {
    load_settings();
    enabled[kind] = !enabled[kind];
}

static void set_text(TimedText* text, s16 timer, u32 color, const char* format, ...) {
    va_list args;
    va_start(args, format);
    SDL_vsnprintf(text->text, sizeof(text->text), format, args);
    va_end(args);
    text->color = color;
    text->timer = timer;
}

void TrainingDisplay_Reset() {
    SDL_zeroa(players);

    for (int i = 0; i < 2; i++) {
        players[i].red_late_from = -1;
        players[i].first_free = -1;
    }

    exchange_active = false;
}

// Frame advantage

/// True on frames where the player is on the ground in a neutral state and could start an attack. The movement code
/// clears `permited_koa` every frame and sets a bit for each kind of attack it checks for, and it skips those checks
/// during hitstop, blockstun, hitstun and attack recovery.
static bool can_act(const PLW* wk) {
    return wk->wu.routine_no[1] == 0 && wk->permited_koa != 0 && wk->wu.xyz[1].disp.pos <= 0;
}

static bool is_being_hit(const PLW* wk) {
    // 1: hit or guarding, 3: being thrown
    return wk->wu.routine_no[1] == 1 || wk->wu.routine_no[1] == 3;
}

static void show_advantage(s16 attacker, s32 advantage) {
    const u32 color = advantage > 0 ? COLOR_GREEN : (advantage < 0 ? COLOR_RED : COLOR_WHITE);

    set_text(&players[attacker].advantage, ADVANTAGE_SHOW_FRAMES, color, "%+d", advantage);
    set_text(&players[attacker ^ 1].advantage, ADVANTAGE_SHOW_FRAMES, color, "%+d", -advantage);
}

static void update_frame_advantage() {
    for (int i = 0; i < 2; i++) {
        const bool hit = is_being_hit(&plw[i]);

        if (hit && !players[i].was_hit) {
            if (exchange_active && exchange_defender != i) {
                exchange_active = false; // Both players got hit: a trade, so there's no advantage to show
            } else {
                if (!exchange_active) {
                    // The attacker may already be able to act, e.g. after a projectile
                    players[i ^ 1].first_free = can_act(&plw[i ^ 1]) ? frame : -1;
                }

                exchange_active = true;
                exchange_defender = i;
            }
        }

        if (hit) {
            players[i].first_free = -1;
        }

        players[i].was_hit = hit;
    }

    if (!exchange_active) {
        return;
    }

    for (int i = 0; i < 2; i++) {
        if (players[i].first_free < 0 && can_act(&plw[i])) {
            players[i].first_free = frame;
        }
    }

    if (players[0].first_free >= 0 && players[1].first_free >= 0) {
        const s16 attacker = exchange_defender ^ 1;
        show_advantage(attacker, players[exchange_defender].first_free - players[attacker].first_free);
        exchange_active = false;
    }
}

// Red parry timing

void TrainingDisplay_NoteRedParryEarly(PLW* wk, s16 frames) {
    if (!is_active()) {
        return;
    }

    set_text(&players[wk->wu.id].red_parry, RED_PARRY_SHOW_FRAMES, COLOR_RED, "RED: %d EARLY", frames);
    players[wk->wu.id].red_late_from = -1;
}

void TrainingDisplay_NoteRedParryChance(PLW* wk) {
    if (!is_active()) {
        return;
    }

    players[wk->wu.id].red_late_from = frame;
}

/// Parry command entries in `waza_flag`: forward, down, air
static const s16 parry_commands[3] = { 3, 4, 5 };

static void update_red_parry_timing() {
    for (int i = 0; i < 2; i++) {
        PlayerDisplay* pd = &players[i];
        const PLW* wk = &plw[i];
        bool pressed = false;

        for (int j = 0; j < 3; j++) {
            const s16 flag = wk->cp->waza_flag[parry_commands[j]];

            // The countdown jumps back up when a new parry input is made
            if (flag > pd->prev_parry_flags[j]) {
                pressed = true;
            }

            pd->prev_parry_flags[j] = flag;
        }

        if (pd->red_late_from < 0) {
            continue;
        }

        const s32 late = frame - pd->red_late_from;

        if (late > RED_PARRY_LATE_WINDOW || !(wk->guard_chuu != 0 && wk->guard_chuu < 5)) {
            pd->red_late_from = -1;
            continue;
        }

        if (pressed && late > 0) {
            set_text(&pd->red_parry, RED_PARRY_SHOW_FRAMES, COLOR_RED, "RED: %d LATE", late);
            pd->red_late_from = -1;
        }
    }
}

// Guard direction

void TrainingDisplay_NoteGuard(PLW* wk) {
    PlayerDisplay* pd;
    u16 lever;

    if (!is_active()) {
        return;
    }

    pd = &players[wk->wu.id];
    lever = wk->cp->sw_lvbt & 0xF;

    if (!(lever & (LEVER_LEFT | LEVER_RIGHT))) {
        // Auto guard blocks without a lever direction, so show the direction that would block it: away from the hit,
        // which the guard state has just turned to face (rl_flag 1 is facing right)
        lever = wk->wu.rl_flag ? LEVER_LEFT : LEVER_RIGHT;

        if (wk->wu.routine_no[2] == 6) {
            lever |= LEVER_DOWN;
        }
    }

    pd->guard_lever = lever;
    pd->guard_timer = GUARD_SHOW_FRAMES;
}

// Stun and juggle

static bool is_stunned(const PLW* wk) {
    return wk->wu.routine_no[1] == 1 && wk->wu.routine_no[2] == 25;
}

static bool is_airborne_in_hitstun(const PLW* wk) {
    // Damage states from 12 up are hit reactions, below that are guard states
    return wk->wu.routine_no[1] == 1 && wk->wu.routine_no[2] >= 12 && wk->wu.xyz[1].disp.pos > 0;
}

/// Remaining juggle time in frames, 0 if there's no limit, or -1 once the window has closed. Each hit on an airborne
/// player sets `zuru_timer` from `sky_dm_zuru_table` (the System Direction AIR COMBO SKILL picks the row, and every
/// juggle hit moves further along it). It counts down by 2 a frame outside hitstop, and once it goes negative the
/// player sets `zuru_flag` and is left out of the hit check, so nothing can hit them until they land.
static s16 juggle_frames_left(const PLW* wk) {
    if (wk->zuru_flag || wk->zuru_timer < 0) {
        return -1;
    }

    return (wk->zuru_timer + 1) / 2;
}

// Charge meter

#define CHARGE_MAX_DIRECTIONS 4
#define CHARGE_GAUGE_SCALE 2 // Pixels per frame, like the lua's gauges
#define CHARGE_GAUGE_HEIGHT 4
#define CHARGE_WINDOW_HEIGHT 2
#define CHARGE_ROW_HEIGHT 19
#define CHARGE_TOP 52
#define CHARGE_SIDE_MARGIN 60

#define COLOR_GAUGE_BACK 0xA0000000
#define COLOR_GAUGE_CHARGING 0xFFE0E0E0
#define COLOR_GAUGE_CHARGED 0xFF40E040
#define COLOR_GAUGE_WINDOW 0xFF40A0FF

static void draw_rect(s16 x, s16 y, s16 w, s16 h, u32 color) {
    PAL_CURSOR pc;
    PAL_CURSOR_P p[4];
    PAL_CURSOR_COL col[4];

    if (w <= 0 || h <= 0) {
        return;
    }

    // Top left, top right, bottom left, bottom right, like the fade quads
    p[0].x = p[2].x = x;
    p[1].x = p[3].x = x + w;
    p[0].y = p[1].y = y;
    p[2].y = p[3].y = y + h;

    for (int i = 0; i < 4; i++) {
        col[i].color = color;
    }

    pc.p = p;
    pc.col = col;
    pc.tex = NULL;
    pc.num = 4;
    njDrawPolygon2D(&pc, 4, PrioBase[2], 0x60);
}

static void draw_gauge(s16 x, s16 y, s16 w, s16 h, s16 value, s16 max, u32 color) {
    draw_rect(x - 1, y - 1, w + 2, h + 2, COLOR_GAUGE_BACK);

    if (max > 0) {
        draw_rect(x, y, w * SDL_clamp(value, 0, max) / max, h, color);
    }
}

static void draw_charge_meter(s16 id) {
    CmdChargeState states[CHARGE_MAX_DIRECTIONS];
    const s32 count = cmd_get_charge_states(id, states, CHARGE_MAX_DIRECTIONS);
    char text[24];
    s16 y = CHARGE_TOP;

    for (s32 i = 0; i < count; i++) {
        const CmdChargeState* st = &states[i];
        const s16 width = st->needed * CHARGE_GAUGE_SCALE;
        const s16 x = (id == 0) ? CHARGE_SIDE_MARGIN : 384 - CHARGE_SIDE_MARGIN - width;
        const char* name = (st->lever & 2) ? "DOWN" : "BACK";

        if (st->charged) {
            SDL_snprintf(text, sizeof(text), "%s  OK  %d", name, st->time_left);
        } else {
            SDL_snprintf(text, sizeof(text), "%s  %d", name, st->needed - st->held);
        }

        SSPutStrPro(0, x, y, 9, st->charged ? COLOR_GREEN : COLOR_WHITE, text);
        const u32 color = st->charged ? COLOR_GAUGE_CHARGED : COLOR_GAUGE_CHARGING;
        const s16 window_y = y + 9 + CHARGE_GAUGE_HEIGHT + 2;

        draw_gauge(x, y + 9, width, CHARGE_GAUGE_HEIGHT, st->held, st->needed, color);
        draw_gauge(x, window_y, width, CHARGE_WINDOW_HEIGHT, st->time_left, st->window, COLOR_GAUGE_WINDOW);
        y += CHARGE_ROW_HEIGHT;
    }
}

// Main entry points

void TrainingDisplay_Update() {
    if (!is_active() || Game_pause || EXE_flag) {
        return;
    }

    frame++;
    update_frame_advantage();
    update_red_parry_timing();

    for (int i = 0; i < 2; i++) {
        PlayerDisplay* pd = &players[i];

        if (pd->advantage.timer > 0) {
            pd->advantage.timer--;
        }

        if (pd->red_parry.timer > 0) {
            pd->red_parry.timer--;
        }

        if (pd->guard_timer > 0) {
            pd->guard_timer--;
        }
    }
}

static void player_label_origin(const PLW* wk, s16* x, s16* y) {
    Vec3 point = { .x = wk->wu.position_x, .y = wk->wu.position_y + base_y_pos + LABEL_HEIGHT, .z = 0 };
    Vec3 screen;
    s16 family = wk->wu.my_family;

    if (family < 0 || family >= (s16)SDL_arraysize(BgMATRIX)) {
        family = 0;
    }

    njCalcPoint(&BgMATRIX[family], &point, &screen);
    *x = SDL_clamp((s32)screen.x, 40, 344);
    *y = SDL_clamp((s32)screen.y, 40, 200);
}

static void draw_centered(s16 x, s16 y, u32 color, const char* text) {
    // SSPutStrPro centres the text in [0, x] when its first argument is set, so pass twice the centre
    SSPutStrPro(1, x * 2, y, 9, color, text);
}

static const GlyphPosition lever_glyphs[16] = {
    [0] = GLYPH_NEUTRAL,
    [LEVER_UP] = GLYPH_UP,
    [LEVER_DOWN] = GLYPH_DOWN,
    [LEVER_LEFT] = GLYPH_LEFT,
    [LEVER_RIGHT] = GLYPH_RIGHT,
    [LEVER_UP | LEVER_LEFT] = GLYPH_UP_LEFT,
    [LEVER_UP | LEVER_RIGHT] = GLYPH_UP_RIGHT,
    [LEVER_DOWN | LEVER_LEFT] = GLYPH_DOWN_LEFT,
    [LEVER_DOWN | LEVER_RIGHT] = GLYPH_DOWN_RIGHT,
};

static void draw_guard(s16 x, s16 y, u16 lever) {
    const char* height = (lever & LEVER_DOWN) ? "LOW" : "HIGH";
    char text[16];

    SDL_snprintf(text, sizeof(text), "GUARD %s", height);
    draw_centered(x - (GLYPH_SIZE + 2) / 2, y, COLOR_YELLOW, text);

    if (lever < SDL_arraysize(lever_glyphs) && (lever & (LEVER_LEFT | LEVER_RIGHT))) {
        glyph_renderer_state.z = PrioBase[2];
        glyph_renderer_state.color = GLYPH_COLOR_WHITE;
        glyph_renderer_state.position.x = x + SSGetDrawSizePro(text) / 2 - (GLYPH_SIZE + 2) / 2 + 2;
        glyph_renderer_state.position.y = y;
        GlyphRenderer_DrawGlyph(lever_glyphs[lever]);
    }
}

void TrainingDisplay_Render() {
    s16 x;
    s16 y;
    char text[16];

    if (!is_active()) {
        return;
    }

    for (int i = 0; i < 2; i++) {
        const PLW* wk = &plw[i];
        const PlayerDisplay* pd = &players[i];

        if (TrainingDisplay_IsEnabled(TRAINING_DISPLAY_CHARGE)) {
            draw_charge_meter(i);
        }

        player_label_origin(wk, &x, &y);

        if (TrainingDisplay_IsEnabled(TRAINING_DISPLAY_FRAME_ADVANTAGE) && pd->advantage.timer > 0) {
            draw_centered(x, y, pd->advantage.color, pd->advantage.text);
            y -= LINE_HEIGHT;
        }

        if (TrainingDisplay_IsEnabled(TRAINING_DISPLAY_RED_PARRY_TIMING) && pd->red_parry.timer > 0) {
            draw_centered(x, y, pd->red_parry.color, pd->red_parry.text);
            y -= LINE_HEIGHT;
        }

        if (TrainingDisplay_IsEnabled(TRAINING_DISPLAY_GUARD_DIRECTION) && pd->guard_timer > 0) {
            draw_guard(x, y, pd->guard_lever);
            y -= LINE_HEIGHT;
        }

        if (TrainingDisplay_IsEnabled(TRAINING_DISPLAY_STUN_TIMER) && is_stunned(wk)) {
            SDL_snprintf(text, sizeof(text), "STUN %d", SDL_max(0, wk->py->time));
            draw_centered(x, y, COLOR_YELLOW, text);
            y -= LINE_HEIGHT;
        }

        if (TrainingDisplay_IsEnabled(TRAINING_DISPLAY_JUGGLE) && is_airborne_in_hitstun(wk)) {
            const s16 left = juggle_frames_left(wk);

            if (left < 0) {
                draw_centered(x, y, COLOR_BLUE, "NO JUGGLE");
            } else if (left == 0) {
                draw_centered(x, y, COLOR_WHITE, "JUGGLE OK");
            } else {
                SDL_snprintf(text, sizeof(text), "JUGGLE %d", left);
                draw_centered(x, y, COLOR_WHITE, text);
            }
        }
    }
}
