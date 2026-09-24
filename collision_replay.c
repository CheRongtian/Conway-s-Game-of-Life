#include "collision_replay.h"

#include <stdlib.h>
#include <string.h>

int collision_replay_initialize(
    CollisionReplay *replay,
    const CollisionSearchCase *search_case)
{
    if (!replay || !search_case || !search_case->has_reaction_bbox ||
        search_case->reaction_bbox.min_x < 0 ||
        search_case->reaction_bbox.min_y < 0 ||
        search_case->reaction_bbox.max_x >= LIFE_WIDTH ||
        search_case->reaction_bbox.max_y >= LIFE_HEIGHT ||
        search_case->reaction_bbox.min_x > search_case->reaction_bbox.max_x ||
        search_case->reaction_bbox.min_y > search_case->reaction_bbox.max_y)
        return 0;

    memset(replay, 0, sizeof(*replay));

    if (!collision_search_case_scenario(
            search_case,
            &replay->scenario))
        return 0;

    replay->frame_count = (size_t)search_case->terminal_generation + 1;

    if (replay->frame_count == 0) return 0;

    replay->frames = malloc(replay->frame_count * sizeof(*replay->frames));

    if (!replay->frames) return 0;

    LifeWorld world;
    life_world_reset(&world);
    LifeBoard *initial_board = life_world_current(&world);
    pattern_place_generation0(
        initial_board,
        replay->scenario.pattern,
        replay->scenario.pattern_origin
    );

    if (!glider_place_generation0(
            initial_board,
            replay->scenario.incoming_glider) ||
        life_live_cell_count(initial_board) !=
            replay->scenario.pattern->live_cell_count + GLIDER_CELL_COUNT)
    {
        collision_replay_destroy(replay);
        return 0;
    }

    for (size_t frame = 0; frame < replay->frame_count; ++frame)
    {
        replay->frames[frame] = *life_world_current_const(&world);

        if (frame + 1 < replay->frame_count)
            life_world_step(&world);
    }

    replay->search_case = search_case;
    replay->reaction_bbox = search_case->reaction_bbox;
    return 1;
}

void collision_replay_destroy(CollisionReplay *replay)
{
    if (!replay) return;

    free(replay->frames);
    memset(replay, 0, sizeof(*replay));
}

const LifeBoard *collision_replay_board(const CollisionReplay *replay)
{
    if (!replay || !replay->frames ||
        replay->current_frame >= replay->frame_count)
        return NULL;

    return &replay->frames[replay->current_frame];
}

uint64_t collision_replay_generation(const CollisionReplay *replay)
{
    return replay ? (uint64_t)replay->current_frame : 0;
}

int collision_replay_next(CollisionReplay *replay)
{
    if (!replay || replay->current_frame + 1 >= replay->frame_count)
        return 0;

    replay->current_frame += 1;
    return 1;
}

int collision_replay_previous(CollisionReplay *replay)
{
    if (!replay || replay->current_frame == 0) return 0;

    replay->current_frame -= 1;
    return 1;
}

void collision_replay_restart(CollisionReplay *replay)
{
    if (!replay) return;

    replay->current_frame = 0;
}

SignalState collision_replay_expected_input(
    const CollisionReplay *replay,
    uint64_t generation)
{
    SignalState expected = replay->scenario.incoming_glider;

    while (expected.generation < generation)
        expected = glider_advance(expected);

    return expected;
}

int collision_replay_output_at_generation(
    const CollisionReplay *replay,
    size_t output_index,
    uint64_t generation,
    SignalState *output)
{
    if (!replay || !replay->search_case || !output ||
        output_index >= replay->search_case->output_glider_count ||
        generation > replay->search_case->terminal_generation)
        return 0;

    SignalState signal = replay->search_case->output_gliders[output_index];

    while (signal.generation > generation)
        signal = glider_rewind(signal);

    if (signal.generation != generation) return 0;

    *output = signal;
    return 1;
}
