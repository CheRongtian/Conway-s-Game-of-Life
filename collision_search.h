#ifndef CONWAY_COLLISION_SEARCH_H
#define CONWAY_COLLISION_SEARCH_H

#include "collision.h"

#include <stddef.h>
#include <stdint.h>

#define COLLISION_SEARCH_RADIUS 4
#define COLLISION_SEARCH_SIMULATION_LIMIT 64

typedef struct
{
    Direction direction;
    GliderPhase phase;
    int offset_x;
    int offset_y;
    CollisionOutcome outcome;
    CollisionPatternState pattern_state;
    CollisionValue pattern_stable;
    CollisionValue incoming_glider_survived;
    CollisionValue debris_present;
    size_t debris_cell_count;
    size_t output_glider_count;
    SignalState output_gliders[OUTCOME_MAX_OUTPUT_GLIDERS];
    uint64_t reaction_start_generation;
    uint64_t reaction_duration;
    uint64_t terminal_generation;
    WorldBox reaction_bbox;
    int has_reaction_bbox;
    size_t occurrences;
} CollisionSearchCase;

typedef struct
{
    size_t tested;
    size_t valid;
    size_t invalid_initial_state;
    size_t filtered_no_approach;
    size_t completed;
    size_t timeout;
    size_t absorbed;
    size_t survived;
    size_t debris;
    size_t debris_present;
    size_t pass_through;
    size_t reflected;
    size_t glider_output;
    size_t multi_glider_output;
    size_t pattern_changed;
    size_t pattern_destroyed;
    size_t duplicate_cases;
    size_t case_count;
    CollisionSearchCase *cases;
} CollisionSearchReport;

int collision_search_eater1(CollisionSearchReport *report);
int collision_search_case_scenario(
    const CollisionSearchCase *search_case,
    CollisionScenario *scenario
);
void collision_search_report_print(const CollisionSearchReport *report);
void collision_search_report_destroy(CollisionSearchReport *report);

#endif
