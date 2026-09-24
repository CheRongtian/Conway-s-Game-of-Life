#include "pattern.h"

int pattern_has_cell(
    const PatternSpec *pattern,
    int x,
    int y)
{
    if (!pattern) return 0;

    for (int i = 0; i < pattern->live_cell_count; ++i)
    {
        if (pattern->cells[i].x == x &&
            pattern->cells[i].y == y)
            return 1;
    }

    return 0;
}

void pattern_place_generation0(
    LifeBoard *board,
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    if (!board || !pattern) return;

    for (int i = 0; i < pattern->live_cell_count; ++i)
    {
        life_set_cell(
            board,
            origin.x + pattern->cells[i].x,
            origin.y + pattern->cells[i].y,
            1
        );
    }
}

int pattern_matches_at(
    const LifeBoard *board,
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    if (!board || !pattern) return 0;

    for (int y = 0; y < pattern->height; ++y)
    {
        for (int x = 0; x < pattern->width; ++x)
        {
            int expected = pattern_has_cell(pattern, x, y);
            int actual = life_cell_is_alive(
                board,
                origin.x + x,
                origin.y + y
            );

            if (actual != expected) return 0;
        }
    }

    return 1;
}

int pattern_board_equals_only(
    const LifeBoard *board,
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    if (!board || !pattern) return 0;

    return life_live_cell_count(board) == pattern->live_cell_count &&
           pattern_matches_at(board, pattern, origin);
}
