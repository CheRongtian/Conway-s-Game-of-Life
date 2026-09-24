#ifndef CONWAY_COLLISION_H
#define CONWAY_COLLISION_H

#include "eater1.h"
#include "glider.h"
#include "life.h"
#include "pattern.h"

#include <stdint.h>

typedef struct
{
    const PatternSpec *pattern;
    WorldAnchor pattern_origin;
    SignalState incoming_glider;
    uint64_t simulation_limit;
} CollisionScenario;

typedef enum
{
    COLLISION_RUN_FAILED,
    COLLISION_RUN_RUNNING,
    COLLISION_RUN_ABSORBED,
    COLLISION_RUN_TIMEOUT
} CollisionRunState;

typedef enum
{
    COLLISION_PHASE_APPROACH,
    COLLISION_PHASE_REACTION
} CollisionPhase;

typedef struct
{
    CollisionScenario scenario;
    LifeWorld world;
    uint64_t generation;
    uint64_t scheduled_input_generation;
    uint64_t reaction_start_generation;
    GliderTracker tracker;
    Eater1Contract eater1_contract;
    CollisionPhase phase;
    CollisionRunState state;
    VerificationResult verification_result;
} CollisionRun;

int collision_build_eater1_scenario(CollisionScenario *scenario);
int collision_run_initialize(
    CollisionRun *run,
    const CollisionScenario *scenario
);
CollisionRunState collision_run_observe(CollisionRun *run);
int collision_run_step(CollisionRun *run);
CollisionRunState collision_run_headless(
    const CollisionScenario *scenario,
    CollisionRun *run
);
const LifeBoard *collision_run_board(const CollisionRun *run);
const char *collision_run_state_name(CollisionRunState state);

#endif
