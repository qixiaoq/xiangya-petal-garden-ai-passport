#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GARDEN_SIDE 6
#define GARDEN_COLORS 5
#define GARDEN_MAX_MOVES 60

typedef struct { uint8_t a, b; } garden_move_t;

typedef struct {
    uint8_t cells[GARDEN_SIDE * GARDEN_SIDE];
    uint32_t rng;
    uint32_t total_cleared;
    uint32_t level;
    uint32_t blooms;
    uint32_t turns;
    uint32_t last_cleared;
    uint32_t last_cascades;
    garden_move_t moves[GARDEN_MAX_MOVES];
    uint8_t move_count;
    uint8_t selected_move;
} garden_game_t;

void garden_init(garden_game_t *game, uint32_t seed);
void garden_rebuild_moves(garden_game_t *game);
void garden_select_next(garden_game_t *game, int direction);
bool garden_play_selected(garden_game_t *game);
bool garden_try_swap(garden_game_t *game, uint8_t a, uint8_t b);
uint8_t garden_preview_swap(const garden_game_t *game, uint8_t a, uint8_t b, bool marks[36]);
bool garden_has_valid_swap(garden_game_t *game);
bool garden_has_match(const garden_game_t *game);
