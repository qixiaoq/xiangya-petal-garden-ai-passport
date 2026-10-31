#include "garden_model.h"

#include <string.h>

static uint32_t random_next(garden_game_t *game) {
    uint32_t x = game->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->rng = x;
    return x;
}

static uint8_t random_flower(garden_game_t *game) {
    return (uint8_t)(1 + random_next(game) % GARDEN_COLORS);
}

static void swap(garden_game_t *game, uint8_t a, uint8_t b) {
    uint8_t value = game->cells[a];
    game->cells[a] = game->cells[b];
    game->cells[b] = value;
}

static uint8_t mark_matches(const garden_game_t *game, bool marks[36]) {
    memset(marks, 0, 36 * sizeof(bool));
    for (int y = 0; y < GARDEN_SIDE; ++y) {
        int x = 0;
        while (x < GARDEN_SIDE) {
            int end = x + 1;
            uint8_t flower = game->cells[y * GARDEN_SIDE + x];
            while (end < GARDEN_SIDE && flower && game->cells[y * GARDEN_SIDE + end] == flower) ++end;
            if (end - x >= 3) for (int p = x; p < end; ++p) marks[y * GARDEN_SIDE + p] = true;
            x = end;
        }
    }
    for (int x = 0; x < GARDEN_SIDE; ++x) {
        int y = 0;
        while (y < GARDEN_SIDE) {
            int end = y + 1;
            uint8_t flower = game->cells[y * GARDEN_SIDE + x];
            while (end < GARDEN_SIDE && flower && game->cells[end * GARDEN_SIDE + x] == flower) ++end;
            if (end - y >= 3) for (int p = y; p < end; ++p) marks[p * GARDEN_SIDE + x] = true;
            y = end;
        }
    }
    uint8_t count = 0;
    for (int i = 0; i < 36; ++i) if (marks[i]) ++count;
    return count;
}

bool garden_has_match(const garden_game_t *game) {
    bool marks[36];
    return mark_matches(game, marks) != 0;
}

void garden_rebuild_moves(garden_game_t *game) {
    garden_move_t previous = {0, 0};
    if (game->selected_move < game->move_count) previous = game->moves[game->selected_move];
    game->move_count = 0;
    game->selected_move = 0;
    for (int i = 0; i < 36; ++i) {
        uint8_t neighbors[2] = { (uint8_t)(i + 1), (uint8_t)(i + GARDEN_SIDE) };
        int count = (i % GARDEN_SIDE != GARDEN_SIDE - 1) + (i / GARDEN_SIDE != GARDEN_SIDE - 1);
        if (i % GARDEN_SIDE == GARDEN_SIDE - 1) neighbors[0] = neighbors[1];
        for (int j = 0; j < count; ++j) {
            uint8_t b = neighbors[j];
            uint8_t pos = game->move_count++;
            game->moves[pos] = (garden_move_t){ (uint8_t)i, b };
            if (previous.a == i && previous.b == b) game->selected_move = pos;
        }
    }
}

static bool adjacent(uint8_t a, uint8_t b) {
    return a < 36 && b < 36 &&
        (b == a + 6 || a == b + 6 ||
         (a / 6 == b / 6 && (b == a + 1 || a == b + 1)));
}

uint8_t garden_preview_swap(const garden_game_t *game, uint8_t a, uint8_t b, bool marks[36]) {
    memset(marks, 0, 36 * sizeof(bool));
    if (!adjacent(a, b)) return 0;
    garden_game_t preview = *game;
    swap(&preview, a, b);
    return mark_matches(&preview, marks);
}

bool garden_has_valid_swap(garden_game_t *game) {
    bool marks[36];
    for (int i = 0; i < game->move_count; ++i) {
        garden_move_t move = game->moves[i];
        if (garden_preview_swap(game, move.a, move.b, marks)) return true;
    }
    return false;
}

static void generate_board(garden_game_t *game) {
    for (int tries = 0; tries < 1000; ++tries) {
        for (int y = 0; y < GARDEN_SIDE; ++y) {
            for (int x = 0; x < GARDEN_SIDE; ++x) {
                uint8_t flower;
                do {
                    flower = random_flower(game);
                } while ((x >= 2 && game->cells[y * 6 + x - 1] == flower && game->cells[y * 6 + x - 2] == flower) ||
                         (y >= 2 && game->cells[(y - 1) * 6 + x] == flower && game->cells[(y - 2) * 6 + x] == flower));
                game->cells[y * 6 + x] = flower;
            }
        }
        garden_rebuild_moves(game);
        if (garden_has_valid_swap(game)) return;
    }
}

void garden_init(garden_game_t *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng = seed ? seed : 0xA5A5BEEFu;
    game->level = 1;
    generate_board(game);
}

void garden_select_next(garden_game_t *game, int direction) {
    if (!game->move_count) return;
    game->selected_move = (uint8_t)((game->selected_move + game->move_count +
                                    (direction < 0 ? -1 : 1)) % game->move_count);
}

bool garden_play_selected(garden_game_t *game) {
    if (!game->move_count || game->selected_move >= game->move_count) return false;
    garden_move_t move = game->moves[game->selected_move];
    return garden_try_swap(game, move.a, move.b);
}

bool garden_try_swap(garden_game_t *game, uint8_t a, uint8_t b) {
    bool preview_marks[36];
    if (!garden_preview_swap(game, a, b, preview_marks)) {
        game->last_cleared = 0;
        game->last_cascades = 0;
        return false;
    }
    swap(game, a, b);
    game->last_cleared = 0;
    game->last_cascades = 0;
    for (int cascade = 0; cascade < 32; ++cascade) {
        bool marks[36];
        uint8_t cleared = mark_matches(game, marks);
        if (!cleared) break;
        game->last_cleared += cleared;
        ++game->last_cascades;
        for (int x = 0; x < GARDEN_SIDE; ++x) {
            int bottom = GARDEN_SIDE - 1;
            for (int y = GARDEN_SIDE - 1; y >= 0; --y) {
                int at = y * GARDEN_SIDE + x;
                if (!marks[at]) game->cells[bottom-- * GARDEN_SIDE + x] = game->cells[at];
            }
            while (bottom >= 0) game->cells[bottom-- * GARDEN_SIDE + x] = random_flower(game);
        }
    }
    ++game->turns;
    game->total_cleared += game->last_cleared;
    game->blooms = game->total_cleared / 3;
    game->level = 1 + game->total_cleared / 18;
    garden_rebuild_moves(game);
    if (!garden_has_valid_swap(game) || garden_has_match(game)) generate_board(game);
    return true;
}
