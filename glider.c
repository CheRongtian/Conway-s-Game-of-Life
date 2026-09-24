#include "glider.h"

static const CellOffset glider_offsets
    [DIRECTION_COUNT][PHASE_COUNT][GLIDER_CELL_COUNT] =
{
    {
        {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}},
        {{0, 0}, {2, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{2, 0}, {0, 1}, {2, 1}, {1, 2}, {2, 2}},
        {{0, 0}, {1, 1}, {2, 1}, {0, 2}, {1, 2}}
    },
    {
        {{1, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2}},
        {{0, 0}, {2, 0}, {0, 1}, {1, 1}, {1, 2}},
        {{0, 0}, {0, 1}, {2, 1}, {0, 2}, {1, 2}},
        {{2, 0}, {0, 1}, {1, 1}, {1, 2}, {2, 2}}
    },
    {
        {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {1, 2}},
        {{1, 0}, {1, 1}, {2, 1}, {0, 2}, {2, 2}},
        {{1, 0}, {2, 0}, {0, 1}, {2, 1}, {2, 2}},
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}, {0, 2}}
    },
    {
        {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {0, 2}, {2, 2}},
        {{0, 0}, {1, 0}, {0, 1}, {2, 1}, {0, 2}},
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 2}}
    }
};

static const CellOffset anchor_delta[DIRECTION_COUNT][PHASE_COUNT] =
{
    {{0, 1}, {0, 0}, {1, 0}, {0, 0}},
    {{0, 1}, {0, 0}, {-1, 0}, {0, 0}},
    {{0, -1}, {0, 0}, {1, 0}, {0, 0}},
    {{0, -1}, {0, 0}, {-1, 0}, {0, 0}}
};

int glider_signal_is_valid(SignalState signal)
{
    return signal.direction >= SE &&
           signal.direction < DIRECTION_COUNT &&
           signal.phase >= P0 &&
           signal.phase < PHASE_COUNT;
}

int glider_get_live_cell(
    SignalState signal,
    int cell_index,
    WorldAnchor *cell)
{
    if (!cell || !glider_signal_is_valid(signal) ||
        cell_index < 0 || cell_index >= GLIDER_CELL_COUNT)
        return 0;

    CellOffset offset =
        glider_offsets[signal.direction][signal.phase][cell_index];
    cell->x = signal.anchor.x + offset.x;
    cell->y = signal.anchor.y + offset.y;
    return 1;
}

const char *glider_direction_name(Direction direction)
{
    switch (direction)
    {
        case SE:
            return "SE";
        case SW:
            return "SW";
        case NE:
            return "NE";
        case NW:
            return "NW";
        case DIRECTION_COUNT:
        default:
            return "UNKNOWN";
    }
}

const char *glider_phase_name(GliderPhase phase)
{
    switch (phase)
    {
        case P0:
            return "P0";
        case P1:
            return "P1";
        case P2:
            return "P2";
        case P3:
            return "P3";
        case PHASE_COUNT:
        default:
            return "UNKNOWN";
    }
}

SignalState glider_advance(SignalState signal)
{
    if (!glider_signal_is_valid(signal)) return signal;

    CellOffset delta = anchor_delta[signal.direction][signal.phase];

    signal.anchor.x += delta.x;
    signal.anchor.y += delta.y;
    signal.phase = (GliderPhase)((signal.phase + 1) % PHASE_COUNT);
    signal.generation += 1;

    return signal;
}

SignalState glider_rewind(SignalState signal)
{
    if (!glider_signal_is_valid(signal) || signal.generation == 0)
        return signal;

    GliderPhase previous_phase =
        (GliderPhase)((signal.phase + PHASE_COUNT - 1) % PHASE_COUNT);
    CellOffset delta = anchor_delta[signal.direction][previous_phase];

    signal.anchor.x -= delta.x;
    signal.anchor.y -= delta.y;
    signal.phase = previous_phase;
    signal.generation -= 1;

    return signal;
}

int glider_states_equal(SignalState a, SignalState b)
{
    return glider_geometry_matches(a, b) &&
           a.generation == b.generation;
}

int glider_geometry_matches(SignalState a, SignalState b)
{
    return a.anchor.x == b.anchor.x &&
           a.anchor.y == b.anchor.y &&
           a.direction == b.direction &&
           a.phase == b.phase;
}

int glider_place_generation0(LifeBoard *board, SignalState signal)
{
    if (!board || !glider_signal_is_valid(signal) ||
        signal.generation != 0)
        return 0;

    for (int i = 0; i < GLIDER_CELL_COUNT; ++i)
    {
        CellOffset offset =
            glider_offsets[signal.direction][signal.phase][i];

        life_set_cell(
            board,
            signal.anchor.x + offset.x,
            signal.anchor.y + offset.y,
            1
        );
    }

    return 1;
}

static int glider_is_expected_cell(
    Direction direction,
    GliderPhase phase,
    int x,
    int y)
{
    for (int i = 0; i < GLIDER_CELL_COUNT; ++i)
    {
        CellOffset offset = glider_offsets[direction][phase][i];

        if (offset.x == x && offset.y == y) return 1;
    }

    return 0;
}

VerificationResult glider_verify(
    const LifeBoard *board,
    SignalState expected,
    uint64_t generation)
{
    if (!board || !glider_signal_is_valid(expected))
        return VERIFY_OUT_OF_EXPECTED_STATE;

    if (generation != expected.generation)
        return VERIFY_WRONG_GENERATION;

    for (int i = 0; i < GLIDER_CELL_COUNT; ++i)
    {
        CellOffset offset =
            glider_offsets[expected.direction][expected.phase][i];

        if (!life_cell_is_alive(
                board,
                expected.anchor.x + offset.x,
                expected.anchor.y + offset.y))
            return VERIFY_MISSING_EXPECTED_LIVE;
    }

    for (int y = 0; y < GLIDER_PATTERN_SIZE; ++y)
    {
        for (int x = 0; x < GLIDER_PATTERN_SIZE; ++x)
        {
            if (glider_is_expected_cell(
                    expected.direction,
                    expected.phase,
                    x,
                    y))
                continue;

            if (life_cell_is_alive(
                    board,
                    expected.anchor.x + x,
                    expected.anchor.y + y))
                return VERIFY_EXTRA_BOUNDING_BOX_LIVE;
        }
    }

    for (int y = -1; y <= GLIDER_PATTERN_SIZE; ++y)
    {
        for (int x = -1; x <= GLIDER_PATTERN_SIZE; ++x)
        {
            if (x >= 0 && x < GLIDER_PATTERN_SIZE &&
                y >= 0 && y < GLIDER_PATTERN_SIZE)
                continue;

            if (life_cell_is_alive(
                    board,
                    expected.anchor.x + x,
                    expected.anchor.y + y))
                return VERIFY_ISOLATION_VIOLATION;
        }
    }

    return VERIFY_OK;
}

void glider_tracker_initialize(
    GliderTracker *tracker,
    SignalState initial)
{
    if (!tracker) return;

    tracker->expected = initial;
    tracker->last_observed_generation =
        GLIDER_GENERATION_UNOBSERVED;
    tracker->last_result = VERIFY_OK;
}

VerificationResult glider_tracker_observe(
    GliderTracker *tracker,
    const LifeBoard *board,
    uint64_t generation,
    int advance_after_observation)
{
    if (!tracker) return VERIFY_OUT_OF_EXPECTED_STATE;

    if (tracker->last_observed_generation == generation)
        return tracker->last_result;

    tracker->last_result = glider_verify(
        board,
        tracker->expected,
        generation
    );
    tracker->last_observed_generation = generation;

    if (tracker->last_result == VERIFY_OK && advance_after_observation)
        tracker->expected = glider_advance(tracker->expected);

    return tracker->last_result;
}
