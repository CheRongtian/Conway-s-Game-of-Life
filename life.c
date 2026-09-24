#include "life.h"

#include <string.h>

int life_wrap_coordinate(int64_t coordinate, int size)
{
    int64_t wrapped = coordinate % size;

    if (wrapped < 0) wrapped += size;

    return (int)wrapped;
}

void life_board_clear(LifeBoard *board)
{
    if (!board) return;

    memset(board, 0, sizeof(*board));
}

void life_world_reset(LifeWorld *world)
{
    if (!world) return;

    memset(world, 0, sizeof(*world));
}

LifeBoard *life_world_current(LifeWorld *world)
{
    if (!world) return NULL;

    return &world->buffers[world->current_index];
}

const LifeBoard *life_world_current_const(const LifeWorld *world)
{
    if (!world) return NULL;

    return &world->buffers[world->current_index];
}

static void life_sync_border(LifeBoard *board)
{
    for (int y = 1; y <= LIFE_HEIGHT; ++y)
    {
        board->cells[y][0] = board->cells[y][LIFE_WIDTH];
        board->cells[y][LIFE_WIDTH + 1] = board->cells[y][1];
    }

    for (int x = 1; x <= LIFE_WIDTH; ++x)
    {
        board->cells[0][x] = board->cells[LIFE_HEIGHT][x];
        board->cells[LIFE_HEIGHT + 1][x] = board->cells[1][x];
    }

    board->cells[0][0] = board->cells[LIFE_HEIGHT][LIFE_WIDTH];
    board->cells[0][LIFE_WIDTH + 1] = board->cells[LIFE_HEIGHT][1];
    board->cells[LIFE_HEIGHT + 1][0] = board->cells[1][LIFE_WIDTH];
    board->cells[LIFE_HEIGHT + 1][LIFE_WIDTH + 1] =
        board->cells[1][1];
}

void life_world_step(LifeWorld *world)
{
    if (!world) return;

    LifeBoard *current = &world->buffers[world->current_index];
    LifeBoard *next = &world->buffers[1 - world->current_index];

    life_sync_border(current);

    for (int y = 1; y <= LIFE_HEIGHT; ++y)
    {
        for (int x = 1; x <= LIFE_WIDTH; ++x)
        {
            int neighbors =
                current->cells[y - 1][x - 1] +
                current->cells[y - 1][x] +
                current->cells[y - 1][x + 1] +
                current->cells[y][x - 1] +
                current->cells[y][x + 1] +
                current->cells[y + 1][x - 1] +
                current->cells[y + 1][x] +
                current->cells[y + 1][x + 1];

            next->cells[y][x] =
                (neighbors == 3) ||
                (current->cells[y][x] && neighbors == 2);
        }
    }

    world->current_index = 1 - world->current_index;
}

int life_cell_is_alive(
    const LifeBoard *board,
    int64_t world_x,
    int64_t world_y)
{
    if (!board) return 0;

    int board_x = life_wrap_coordinate(world_x, LIFE_WIDTH);
    int board_y = life_wrap_coordinate(world_y, LIFE_HEIGHT);

    return board->cells[board_y + 1][board_x + 1] != 0;
}

void life_set_cell(
    LifeBoard *board,
    int64_t world_x,
    int64_t world_y,
    int alive)
{
    if (!board) return;

    int board_x = life_wrap_coordinate(world_x, LIFE_WIDTH);
    int board_y = life_wrap_coordinate(world_y, LIFE_HEIGHT);

    board->cells[board_y + 1][board_x + 1] = alive != 0;
}

int life_live_cell_count(const LifeBoard *board)
{
    if (!board) return 0;

    int count = 0;

    for (int y = 1; y <= LIFE_HEIGHT; ++y)
    {
        for (int x = 1; x <= LIFE_WIDTH; ++x)
            count += board->cells[y][x] != 0;
    }

    return count;
}
