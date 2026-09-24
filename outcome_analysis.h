#ifndef CONWAY_OUTCOME_ANALYSIS_H
#define CONWAY_OUTCOME_ANALYSIS_H

#include "glider.h"
#include "life.h"
#include "pattern.h"

#include <stddef.h>
#include <stdint.h>

#define OUTCOME_REACTION_MARGIN 4
#define OUTCOME_STABILITY_GENERATIONS 16
#define OUTCOME_MAX_OUTPUT_GLIDERS 16

typedef enum
{
    COLLISION_OUTCOME_UNKNOWN,
    COLLISION_OUTCOME_ABSORB,
    COLLISION_OUTCOME_PASS_THROUGH,
    COLLISION_OUTCOME_REFLECT,
    COLLISION_OUTCOME_GLIDER_OUTPUT,
    COLLISION_OUTCOME_MULTI_GLIDER_OUTPUT,
    COLLISION_OUTCOME_PATTERN_CHANGED,
    COLLISION_OUTCOME_PATTERN_DESTROYED,
    COLLISION_OUTCOME_DEBRIS,
    COLLISION_OUTCOME_TIMEOUT,
    COLLISION_OUTCOME_FAILED
} CollisionOutcome;

typedef enum
{
    COLLISION_PATTERN_UNKNOWN,
    COLLISION_PATTERN_RESTORED,
    COLLISION_PATTERN_CHANGED,
    COLLISION_PATTERN_DESTROYED
} CollisionPatternState;

typedef enum
{
    COLLISION_VALUE_UNKNOWN = -1,
    COLLISION_VALUE_NO = 0,
    COLLISION_VALUE_YES = 1
} CollisionValue;

typedef struct
{
    const LifeBoard *terminal_board;
    const PatternSpec *pattern;
    WorldAnchor pattern_origin;
    SignalState incoming_glider;
    SignalState expected_incoming_glider;
    uint64_t terminal_generation;
    WorldBox reaction_region;
    int timed_out;
} OutcomeAnalysisInput;

typedef struct
{
    CollisionOutcome outcome;
    CollisionPatternState pattern_state;
    CollisionValue pattern_stable;
    CollisionValue incoming_glider_survived;
    CollisionValue debris_present;
    size_t debris_cell_count;
    size_t output_glider_count;
    SignalState output_gliders[OUTCOME_MAX_OUTPUT_GLIDERS];
} OutcomeAnalysis;

int outcome_analyze(
    const OutcomeAnalysisInput *input,
    OutcomeAnalysis *analysis
);
const char *collision_outcome_name(CollisionOutcome outcome);
const char *collision_pattern_state_name(CollisionPatternState state);
const char *collision_value_name(CollisionValue value);

#endif
