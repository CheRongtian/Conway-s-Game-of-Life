#ifndef CONWAY_PATTERN_H
#define CONWAY_PATTERN_H

#include "life.h"

typedef struct
{
    int pattern_id;
    int width;
    int height;
    int live_cell_count;
    const CellOffset *cells;
} PatternSpec;

int pattern_has_cell(
    const PatternSpec *pattern,
    int x,
    int y
);
void pattern_place_generation0(
    LifeBoard *board,
    const PatternSpec *pattern,
    WorldAnchor origin
);
int pattern_matches_at(
    const LifeBoard *board,
    const PatternSpec *pattern,
    WorldAnchor origin
);
int pattern_board_equals_only(
    const LifeBoard *board,
    const PatternSpec *pattern,
    WorldAnchor origin
);

#endif
