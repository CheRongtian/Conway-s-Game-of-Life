#ifndef CONWAY_COLLISION_H
#define CONWAY_COLLISION_H

#include "eater1.h"
#include "glider.h"
#include "life.h"
#include "outcome_analysis.h"
#include "pattern.h"

#include <stddef.h>
#include <stdint.h>

#define COLLISION_REACTION_CONTEXT_MARGIN 4
#define COLLISION_REACTION_MAX_FRAMES EATER1_REACTION_MAX_FRAMES
#define COLLISION_REACTION_MAX_WIDTH \
    (EATER1_REACTION_MAX_WIDTH + 2 * COLLISION_REACTION_CONTEXT_MARGIN)
#define COLLISION_REACTION_MAX_HEIGHT \
    (EATER1_REACTION_MAX_HEIGHT + 2 * COLLISION_REACTION_CONTEXT_MARGIN)

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
    uint64_t initial_generation;
    uint64_t first_interaction_generation;
    uint64_t reaction_start_generation;
    uint64_t terminal_generation;
    int has_first_interaction;
    int has_reaction_start;
    int has_terminal;
    WorldBox reaction_bbox;
    int has_reaction_bbox;
} ReactionTimeline;

typedef struct
{
    size_t frame_count;
    int width;
    int height;
    uint64_t first_generation;
    uint64_t last_generation;
    uint64_t last_recorded_generation;
    WorldBox bbox;
    uint8_t cells
        [COLLISION_REACTION_MAX_FRAMES]
        [COLLISION_REACTION_MAX_HEIGHT]
        [COLLISION_REACTION_MAX_WIDTH];
} ReactionRecording;

typedef struct
{
    CollisionOutcome outcome;
    CollisionPatternState pattern_state;
    CollisionValue pattern_stable;
    uint64_t generation_count;
    uint64_t reaction_duration;
    CollisionValue pattern_restored;
    CollisionValue pattern_changed;
    CollisionValue incoming_glider_survived;
    CollisionValue debris_present;
    size_t debris_cell_count;
    size_t output_glider_count;
    SignalState output_gliders[OUTCOME_MAX_OUTPUT_GLIDERS];
    int initial_live_cells;
    int final_live_cells;
    ReactionTimeline timeline;
    ReactionRecording reaction;
} CollisionResult;

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
    CollisionResult result;
    int result_ready;
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
const CollisionResult *collision_run_result(const CollisionRun *run);
void collision_result_print(const CollisionResult *result);
const char *collision_run_state_name(CollisionRunState state);

#endif
