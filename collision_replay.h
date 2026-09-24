#ifndef CONWAY_COLLISION_REPLAY_H
#define CONWAY_COLLISION_REPLAY_H

#include "collision_search.h"

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    CollisionScenario scenario;
    const CollisionSearchCase *search_case;
    LifeBoard *frames;
    size_t frame_count;
    size_t current_frame;
    WorldBox reaction_bbox;
} CollisionReplay;

int collision_replay_initialize(
    CollisionReplay *replay,
    const CollisionSearchCase *search_case
);
void collision_replay_destroy(CollisionReplay *replay);
const LifeBoard *collision_replay_board(const CollisionReplay *replay);
uint64_t collision_replay_generation(const CollisionReplay *replay);
int collision_replay_next(CollisionReplay *replay);
int collision_replay_previous(CollisionReplay *replay);
void collision_replay_restart(CollisionReplay *replay);
SignalState collision_replay_expected_input(
    const CollisionReplay *replay,
    uint64_t generation
);
int collision_replay_output_at_generation(
    const CollisionReplay *replay,
    size_t output_index,
    uint64_t generation,
    SignalState *output
);

#endif
