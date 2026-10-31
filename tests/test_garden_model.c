#include "garden_model.h"

#include <assert.h>
#include <stdio.h>

static void check_board(const garden_game_t *game) {
    assert(!garden_has_match(game));
    assert(game->move_count == GARDEN_MAX_MOVES);
    garden_game_t copy = *game;
    assert(garden_has_valid_swap(&copy));
    assert(game->selected_move < game->move_count);
    for (int i = 0; i < 36; ++i) assert(game->cells[i] >= 1 && game->cells[i] <= GARDEN_COLORS);
    for (int i = 0; i < game->move_count; ++i) {
        garden_move_t move = game->moves[i];
        assert(move.a < 36 && move.b < 36);
        assert(move.b == move.a + 6 || (move.b == move.a + 1 && move.a / 6 == move.b / 6));
    }
}

int main(void) {
    for (unsigned seed = 0; seed < 100; ++seed) {
        garden_game_t game;
        garden_init(&game, seed);
        check_board(&game);
        uint32_t before_invalid = game.total_cleared;
        uint8_t before_cell = game.cells[0];
        assert(!garden_try_swap(&game, 0, 35));
        assert(game.total_cleared == before_invalid && game.cells[0] == before_cell);
        for (int turn = 0; turn < 200; ++turn) {
            uint32_t before = game.total_cleared;
            uint8_t first_valid = 255;
            uint8_t first_invalid = 255;
            bool marks[36];
            for (uint8_t i = 0; i < game.move_count; ++i) {
                garden_move_t move = game.moves[i];
                if (garden_preview_swap(&game, move.a, move.b, marks)) {
                    if (first_valid == 255) first_valid = i;
                } else if (first_invalid == 255) first_invalid = i;
            }
            assert(first_valid != 255 && first_invalid != 255);
            garden_move_t invalid = game.moves[first_invalid];
            assert(!garden_try_swap(&game, invalid.a, invalid.b));
            assert(game.total_cleared == before && game.turns == (uint32_t)turn);
            game.selected_move = first_valid;
            assert(garden_play_selected(&game));
            assert(game.total_cleared >= before + 3);
            assert(game.blooms == game.total_cleared / 3);
            assert(game.level == 1 + game.total_cleared / 18);
            assert(game.turns == (uint32_t)(turn + 1));
            check_board(&game);
        }
        garden_init(&game, seed + 1000);
        assert(game.total_cleared == 0 && game.turns == 0 && game.blooms == 0 && game.level == 1);
        check_board(&game);
    }
    puts("garden model: PASS (20,000 moves)");
}
