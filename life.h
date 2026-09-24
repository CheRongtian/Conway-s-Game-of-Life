#ifndef CONWAY_LIFE_H
#define CONWAY_LIFE_H

#include <stdint.h>

#define LIFE_WIDTH 512
#define LIFE_HEIGHT 256

typedef struct
{
    int64_t x;
    int64_t y;
} WorldAnchor;

typedef struct
{
    int x;
    int y;
} CellOffset;

typedef struct
{
    int64_t min_x;
    int64_t min_y;
    int64_t max_x;
    int64_t max_y;
} WorldBox;

typedef struct
{
    uint8_t cells[LIFE_HEIGHT + 2][LIFE_WIDTH + 2];
} LifeBoard;

typedef struct
{
    LifeBoard buffers[2];
    int current_index;
} LifeWorld;

int life_wrap_coordinate(int64_t coordinate, int size);
void life_board_clear(LifeBoard *board);
void life_world_reset(LifeWorld *world);
LifeBoard *life_world_current(LifeWorld *world);
const LifeBoard *life_world_current_const(const LifeWorld *world);
void life_world_step(LifeWorld *world);
int life_cell_is_alive(
    const LifeBoard *board,
    int64_t world_x,
    int64_t world_y
);
void life_set_cell(
    LifeBoard *board,
    int64_t world_x,
    int64_t world_y,
    int alive
);
int life_live_cell_count(const LifeBoard *board);

#endif
