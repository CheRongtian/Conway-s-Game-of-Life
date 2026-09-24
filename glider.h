#ifndef CONWAY_GLIDER_H
#define CONWAY_GLIDER_H

#include "life.h"

#include <stdint.h>

#define GLIDER_CELL_COUNT 5
#define GLIDER_PATTERN_SIZE 3
#define GLIDER_GENERATION_UNOBSERVED UINT64_MAX

typedef enum
{
    SE,
    SW,
    NE,
    NW,
    DIRECTION_COUNT
} Direction;

typedef enum
{
    P0,
    P1,
    P2,
    P3,
    PHASE_COUNT
} GliderPhase;

typedef struct
{
    WorldAnchor anchor;
    Direction direction;
    GliderPhase phase;
    uint64_t generation;
} SignalState;

typedef enum
{
    VERIFY_OK,
    VERIFY_WRONG_GENERATION,
    VERIFY_MISSING_EXPECTED_LIVE,
    VERIFY_EXTRA_BOUNDING_BOX_LIVE,
    VERIFY_ISOLATION_VIOLATION,
    VERIFY_OUT_OF_EXPECTED_STATE,
    VERIFY_COMPONENT_CONTRACT_MISMATCH,
    VERIFY_EATER_PATTERN_MISMATCH,
    VERIFY_REACTION_FRAME_MISMATCH,
    VERIFY_COMPONENT_SAFETY_VIOLATION,
    VERIFY_RECORDING_CAPACITY_EXCEEDED
} VerificationResult;

typedef struct
{
    SignalState expected;
    uint64_t last_observed_generation;
    VerificationResult last_result;
} GliderTracker;

SignalState glider_advance(SignalState signal);
SignalState glider_rewind(SignalState signal);
int glider_signal_is_valid(SignalState signal);
int glider_get_live_cell(
    SignalState signal,
    int cell_index,
    WorldAnchor *cell
);
const char *glider_direction_name(Direction direction);
const char *glider_phase_name(GliderPhase phase);
int glider_states_equal(SignalState a, SignalState b);
int glider_geometry_matches(SignalState a, SignalState b);
int glider_place_generation0(LifeBoard *board, SignalState signal);
VerificationResult glider_verify(
    const LifeBoard *board,
    SignalState expected,
    uint64_t generation
);
void glider_tracker_initialize(
    GliderTracker *tracker,
    SignalState initial
);
VerificationResult glider_tracker_observe(
    GliderTracker *tracker,
    const LifeBoard *board,
    uint64_t generation,
    int advance_after_observation
);

#endif
