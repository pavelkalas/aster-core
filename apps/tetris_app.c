/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 */

/*
 * Testovaci hra Tetris napsana nad Aster App lifecycle API.
 * Ukazuje jednoduchy update/draw loop, render helpery a klavesnicovy vstup.
 */

#include "aster_api.h"

#define TETRIS_BOARD_MAX_H 24
#define TETRIS_BOARD_MAX_W 26

#define TETRIS_SHAPES 7

typedef struct {
    int board_size;
    int board_w;
    int board_h;
    int offset_x;
    int offset_y;
    u8 board[TETRIS_BOARD_MAX_H][TETRIS_BOARD_MAX_W];
    int piece_id;
    u64 piece_counter;
    int piece_x;
    int piece_y;
    int score;
    int difficulty;
    int slow_mode;
    int base_drop_ticks;
    int base_drop_frames;
    u64 last_drop_tick;
    u64 last_drop_frame;
    int drop_every_ticks;
    int drop_every_frames;
    int game_over;
} tetris_state_t;

static const u8 g_tetris_shapes[TETRIS_SHAPES][4][4] = {
    {
        {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {
        {0, 1, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {
        {0, 1, 0, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {
        {1, 0, 0, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {
        {0, 0, 1, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {
        {0, 1, 1, 0},
        {1, 1, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {
        {1, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    }
};

static const char g_tetris_shape_char[TETRIS_SHAPES] = {'I', 'O', 'T', 'L', 'J', 'S', 'Z'};
static const u8 g_tetris_shape_color[TETRIS_SHAPES] = {0x0B, 0x0E, 0x0D, 0x06, 0x09, 0x0A, 0x0C};

static void tetris_apply_speed_profile(tetris_state_t *s) {
    if (!s) {
        return;
    }

    s->drop_every_ticks = s->base_drop_ticks;
    s->drop_every_frames = s->base_drop_frames;

    if (s->slow_mode) {
        s->drop_every_ticks *= 3;
        s->drop_every_frames *= 3;
    }
}

static void tetris_set_difficulty(tetris_state_t *s, int difficulty) {
    if (!s) {
        return;
    }

    s->difficulty = difficulty;

    if (difficulty == 1) {
        s->base_drop_ticks = 24;
        s->base_drop_frames = 18;
    } else if (difficulty == 2) {
        s->base_drop_ticks = 14;
        s->base_drop_frames = 11;
    } else if (difficulty == 3) {
        s->base_drop_ticks = 6;
        s->base_drop_frames = 5;
    } else {
        s->base_drop_ticks = 3;
        s->base_drop_frames = 2;
    }

    tetris_apply_speed_profile(s);
}

static int tetris_prompt_difficulty(void) {
    for (;;) {
        int k;
        (void)aster_api_clear();
        (void)aster_api_print_line("TETRIS - vyber obtiznost:");
        (void)aster_api_print_line("1 = lehka");
        (void)aster_api_print_line("2 = stredni");
        (void)aster_api_print_line("3 = tezka");
        (void)aster_api_print_line("4 = impossible");
        (void)aster_api_print_line("");
        (void)aster_api_print_line("Stiskni 1-4:");

        k = aster_api_read_key();
        if (k == '1' || k == '2' || k == '3' || k == '4') {
            return k - '0';
        }
    }
}

static int tetris_prompt_board_size(void) {
    for (;;) {
        int k;
        (void)aster_api_clear();
        (void)aster_api_print_line("TETRIS - vyber velikost pole:");
        (void)aster_api_print_line("1 = male (12x14)");
        (void)aster_api_print_line("2 = stredni (16x18)");
        (void)aster_api_print_line("3 = velke (20x22)");
        (void)aster_api_print_line("");
        (void)aster_api_print_line("Stiskni 1-3:");

        k = aster_api_read_key();
        if (k == '1' || k == '2' || k == '3') {
            return k - '0';
        }
    }
}

static void tetris_draw_num(int x, int y, int value, u8 fg, u8 bg) {
    char out[16];
    char tmp[16];
    int i = 0;
    int j = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        (void)aster_api_render_text(x, y, out, fg, bg);
        return;
    }

    while (value > 0 && i < 15) {
        tmp[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }
    out[j] = '\0';
    (void)aster_api_render_text(x, y, out, fg, bg);
}

static int tetris_piece_cell_at(const tetris_state_t *s, int px, int py) {
    if (!s) {
        return 0;
    }

    if (s->piece_id < 0 || s->piece_id >= TETRIS_SHAPES) {
        return 0;
    }

    if (px < 0 || px >= 4 || py < 0 || py >= 4) {
        return 0;
    }

    return g_tetris_shapes[s->piece_id][py][px] ? 1 : 0;
}

static int tetris_collides(const tetris_state_t *s, int base_x, int base_y) {
    int y;
    int x;

    for (y = 0; y < 4; ++y) {
        for (x = 0; x < 4; ++x) {
            int bx;
            int by;
            if (!tetris_piece_cell_at(s, x, y)) {
                continue;
            }

            bx = base_x + x;
            by = base_y + y;

            if (bx < 0 || bx >= s->board_w || by >= s->board_h) {
                return 1;
            }

            if (by >= 0 && s->board[by][bx]) {
                return 1;
            }
        }
    }

    return 0;
}

static void tetris_lock_piece(tetris_state_t *s) {
    int y;
    int x;

    for (y = 0; y < 4; ++y) {
        for (x = 0; x < 4; ++x) {
            int bx;
            int by;
            if (!tetris_piece_cell_at(s, x, y)) {
                continue;
            }

            bx = s->piece_x + x;
            by = s->piece_y + y;
            if (bx >= 0 && bx < s->board_w && by >= 0 && by < s->board_h) {
                s->board[by][bx] = (u8)(s->piece_id + 1);
            }
        }
    }
}

static void tetris_clear_lines(tetris_state_t *s) {
    int y;

    for (y = s->board_h - 1; y >= 0; --y) {
        int x;
        int full = 1;

        for (x = 0; x < s->board_w; ++x) {
            if (!s->board[y][x]) {
                full = 0;
                break;
            }
        }

        if (full) {
            int yy;
            for (yy = y; yy > 0; --yy) {
                for (x = 0; x < s->board_w; ++x) {
                    s->board[yy][x] = s->board[yy - 1][x];
                }
            }
            for (x = 0; x < s->board_w; ++x) {
                s->board[0][x] = 0;
            }
            s->score += 100;
            ++y;
        }
    }
}

static void tetris_spawn_piece(tetris_state_t *s) {
    s->piece_id = (int)((s->piece_counter++) % TETRIS_SHAPES);
    s->piece_x = (s->board_w / 2) - 2;
    s->piece_y = 0;
    if (tetris_collides(s, s->piece_x, s->piece_y)) {
        s->game_over = 1;
    }
}

static int tetris_initial(aster_api_app_state_t *state, void *user) {
    tetris_state_t *s = (tetris_state_t *)user;
    int y;
    int x;

    (void)state;

    if (s->board_size == 1) {
        s->board_w = 12;
        s->board_h = 14;
    } else if (s->board_size == 2) {
        s->board_w = 16;
        s->board_h = 18;
    } else {
        s->board_w = 20;
        s->board_h = 22;
    }
    s->offset_x = 2;
    s->offset_y = 2;
    s->score = 0;
    s->slow_mode = 0;
    tetris_apply_speed_profile(s);
    s->game_over = 0;

    if (s->board_w > TETRIS_BOARD_MAX_W || s->board_h > TETRIS_BOARD_MAX_H) {
        return ASTER_API_APP_ERROR;
    }

    (void)aster_api_clear();

    for (y = 0; y < s->board_h; ++y) {
        for (x = 0; x < s->board_w; ++x) {
            s->board[y][x] = 0;
        }
    }

    tetris_spawn_piece(s);
    s->last_drop_tick = aster_api_ticks_now();
    s->last_drop_frame = state ? state->frame : 0;

    return ASTER_API_APP_CONTINUE;
}

static int tetris_update(aster_api_app_state_t *state, void *user) {
    tetris_state_t *s = (tetris_state_t *)user;
    int key = aster_api_try_read_key();
    u64 now = aster_api_ticks_now();
    int should_drop = 0;

    if (key == 'q' || key == 'Q') {
        return ASTER_API_APP_CLOSE;
    }

    if (key == 'p' || key == 'P') {
        s->slow_mode = s->slow_mode ? 0 : 1;
        tetris_apply_speed_profile(s);
    }

    if (s->game_over) {
        if (key == 'r' || key == 'R') {
            (void)tetris_initial(state, user);
        }
        return ASTER_API_APP_CONTINUE;
    }

    if (key == ASTER_API_KEY_LEFT || key == 'a' || key == 'A') {
        if (!tetris_collides(s, s->piece_x - 1, s->piece_y)) {
            s->piece_x -= 1;
        }
    } else if (key == ASTER_API_KEY_RIGHT || key == 'd' || key == 'D') {
        if (!tetris_collides(s, s->piece_x + 1, s->piece_y)) {
            s->piece_x += 1;
        }
    } else if (key == ASTER_API_KEY_DOWN || key == 's' || key == 'S') {
        if (!tetris_collides(s, s->piece_x, s->piece_y + 1)) {
            s->piece_y += 1;
        }
    }

    if ((now - s->last_drop_tick) > 0) {
        if ((now - s->last_drop_tick) >= (u64)s->drop_every_ticks) {
            should_drop = 1;
            s->last_drop_tick = now;
            s->last_drop_frame = state->frame;
        }
    } else if ((state->frame - s->last_drop_frame) >= (u64)s->drop_every_frames) {
        should_drop = 1;
        s->last_drop_frame = state->frame;
    }

    if (should_drop) {
        s->last_drop_tick = now;

        if (!tetris_collides(s, s->piece_x, s->piece_y + 1)) {
            s->piece_y += 1;
        } else {
            tetris_lock_piece(s);
            tetris_clear_lines(s);
            tetris_spawn_piece(s);
        }
    }

    return ASTER_API_APP_CONTINUE;
}

static int tetris_draw(aster_api_app_state_t *state, void *user) {
    tetris_state_t *s = (tetris_state_t *)user;
    int hud_x = s->offset_x + s->board_w + 4;
    int y;
    int x;

    (void)state;

    (void)aster_api_render_border(s->offset_x - 1, s->offset_y - 1, s->board_w + 2, s->board_h + 2, 0x0F, 0x00);

    for (y = 0; y < s->board_h; ++y) {
        for (x = 0; x < s->board_w; ++x) {
            if (s->board[y][x]) {
                int id = (int)(s->board[y][x] - 1);
                if (id < 0 || id >= TETRIS_SHAPES) {
                    id = 0;
                }
                (void)aster_api_render_char(s->offset_x + x, s->offset_y + y, g_tetris_shape_char[id], g_tetris_shape_color[id], 0x00);
            } else {
                (void)aster_api_render_char(s->offset_x + x, s->offset_y + y, '.', 0x01, 0x00);
            }
        }
    }

    for (y = 0; y < 4; ++y) {
        for (x = 0; x < 4; ++x) {
            if (!tetris_piece_cell_at(s, x, y)) {
                continue;
            }
            (void)aster_api_render_char(s->offset_x + s->piece_x + x, s->offset_y + s->piece_y + y,
                                        g_tetris_shape_char[s->piece_id],
                                        g_tetris_shape_color[s->piece_id],
                                        0x00);
        }
    }

    (void)aster_api_render_text(hud_x, 2, "TETRIS demo", 0x0E, 0x00);
    (void)aster_api_render_text(hud_x, 4, "A/D nebo sipky: pohyb", 0x07, 0x00);
    (void)aster_api_render_text(hud_x, 5, "S nebo dolu: rychleji", 0x07, 0x00);
    (void)aster_api_render_text(hud_x, 6, "Q: konec", 0x07, 0x00);
    (void)aster_api_render_text(hud_x, 7, "P: slow mode", 0x07, 0x00);
    (void)aster_api_render_text(hud_x, 8, "Skore:", 0x0F, 0x00);
    tetris_draw_num(hud_x + 7, 8, s->score, 0x0F, 0x00);

    (void)aster_api_render_text(hud_x, 9, "Obtiznost:", 0x07, 0x00);
    if (s->difficulty == 1) {
        (void)aster_api_render_text(hud_x + 11, 9, "lehka", 0x0A, 0x00);
    } else if (s->difficulty == 2) {
        (void)aster_api_render_text(hud_x + 11, 9, "stredni", 0x0E, 0x00);
    } else if (s->difficulty == 3) {
        (void)aster_api_render_text(hud_x + 11, 9, "tezka", 0x0C, 0x00);
    } else {
        (void)aster_api_render_text(hud_x + 11, 9, "impossible", 0x0D, 0x00);
    }

    (void)aster_api_render_text(hud_x, 10, "Slow:", 0x07, 0x00);
    if (s->slow_mode) {
        (void)aster_api_render_text(hud_x + 6, 10, "ON", 0x0A, 0x00);
    } else {
        (void)aster_api_render_text(hud_x + 6, 10, "OFF", 0x08, 0x00);
    }

    if (s->game_over) {
        (void)aster_api_render_text(hud_x, 12, "GAME OVER", 0x0C, 0x00);
        (void)aster_api_render_text(hud_x, 13, "R: restart", 0x0F, 0x00);
    }

    return ASTER_API_APP_CONTINUE;
}

static int tetris_closing(aster_api_app_state_t *state, void *user) {
    (void)state;
    (void)user;

    (void)aster_api_clear();
    (void)aster_api_print_line("[tetris] v1.0 by Pavel Kalas 2026 <3");
    (void)aster_api_print_line("[tetris] ukonceno");
    return ASTER_API_APP_CONTINUE;
}

void app_tetris_main(void) {
    tetris_state_t state;
    aster_api_app_callbacks_t callbacks;

    state.difficulty = tetris_prompt_difficulty();
    state.board_size = tetris_prompt_board_size();
    state.slow_mode = 0;
    state.piece_counter = 0;
    tetris_set_difficulty(&state, state.difficulty);

    callbacks.initial = tetris_initial;
    callbacks.update = tetris_update;
    callbacks.draw = tetris_draw;
    callbacks.closing = tetris_closing;

    (void)aster_api_app_run(&callbacks, &state, 30);
}
