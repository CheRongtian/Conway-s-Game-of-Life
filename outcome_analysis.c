#include "outcome_analysis.h"

#include <string.h>

static int outcome_pattern_live_cells(
    const LifeBoard *board,
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    int live_cells = 0;

    for (int y = 0; y < pattern->height; ++y)
    {
        for (int x = 0; x < pattern->width; ++x)
            live_cells += life_cell_is_alive(
                board,
                origin.x + x,
                origin.y + y
            );
    }

    return live_cells;
}

static CollisionPatternState outcome_pattern_state(
    const LifeBoard *board,
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    if (pattern_matches_at(board, pattern, origin))
        return COLLISION_PATTERN_RESTORED;

    return outcome_pattern_live_cells(board, pattern, origin) == 0
        ? COLLISION_PATTERN_DESTROYED
        : COLLISION_PATTERN_CHANGED;
}

static int outcome_pattern_region_equal(
    const LifeBoard *first,
    const LifeBoard *second,
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    for (int y = 0; y < pattern->height; ++y)
    {
        for (int x = 0; x < pattern->width; ++x)
        {
            if (life_cell_is_alive(first, origin.x + x, origin.y + y) !=
                life_cell_is_alive(second, origin.x + x, origin.y + y))
                return 0;
        }
    }

    return 1;
}

static int outcome_pattern_is_stable(
    const OutcomeAnalysisInput *input)
{
    LifeWorld probe;
    life_world_reset(&probe);
    *life_world_current(&probe) = *input->terminal_board;

    for (int generation = 0;
         generation < OUTCOME_STABILITY_GENERATIONS;
         ++generation)
    {
        const LifeBoard *previous = life_world_current_const(&probe);
        life_world_step(&probe);

        if (!outcome_pattern_region_equal(
                previous,
                life_world_current_const(&probe),
                input->pattern,
                input->pattern_origin))
            return 0;
    }

    return 1;
}

static int outcome_glider_outside_region(
    SignalState signal,
    WorldBox region)
{
    int64_t min_x = signal.anchor.x;
    int64_t min_y = signal.anchor.y;
    int64_t max_x = min_x + GLIDER_PATTERN_SIZE - 1;
    int64_t max_y = min_y + GLIDER_PATTERN_SIZE - 1;

    return max_x < region.min_x || min_x > region.max_x ||
           max_y < region.min_y || min_y > region.max_y;
}

static int outcome_glider_already_found(
    const OutcomeAnalysis *analysis,
    SignalState candidate)
{
    for (size_t i = 0; i < analysis->output_glider_count; ++i)
    {
        if (glider_geometry_matches(
                analysis->output_gliders[i],
                candidate))
            return 1;
    }

    return 0;
}

static int outcome_find_output_gliders(
    const OutcomeAnalysisInput *input,
    OutcomeAnalysis *analysis)
{
    for (int y = 0; y < LIFE_HEIGHT; ++y)
    {
        for (int x = 0; x < LIFE_WIDTH; ++x)
        {
            if (!input->terminal_board->cells[y + 1][x + 1]) continue;

            for (int direction = SE;
                 direction < DIRECTION_COUNT;
                 ++direction)
            {
                for (int phase = P0; phase < PHASE_COUNT; ++phase)
                {
                    for (int cell_index = 0;
                         cell_index < GLIDER_CELL_COUNT;
                         ++cell_index)
                    {
                        SignalState template =
                        {
                            {0, 0},
                            (Direction)direction,
                            (GliderPhase)phase,
                            input->terminal_generation
                        };
                        WorldAnchor offset;

                        if (!glider_get_live_cell(
                                template,
                                cell_index,
                                &offset))
                            return 0;

                        SignalState candidate = template;
                        candidate.anchor.x = x - offset.x;
                        candidate.anchor.y = y - offset.y;

                        if (!outcome_glider_outside_region(
                                candidate,
                                input->reaction_region) ||
                            glider_verify(
                                input->terminal_board,
                                candidate,
                                input->terminal_generation) != VERIFY_OK ||
                            outcome_glider_already_found(
                                analysis,
                                candidate))
                            continue;

                        if (analysis->output_glider_count >=
                            OUTCOME_MAX_OUTPUT_GLIDERS)
                            return 0;

                        analysis->output_gliders
                            [analysis->output_glider_count] = candidate;
                        analysis->output_glider_count += 1;
                    }
                }
            }
        }
    }

    return 1;
}

static void outcome_mark_pattern_cells(
    uint8_t explained[LIFE_HEIGHT][LIFE_WIDTH],
    const OutcomeAnalysisInput *input)
{
    for (int y = 0; y < input->pattern->height; ++y)
    {
        for (int x = 0; x < input->pattern->width; ++x)
        {
            int world_x = life_wrap_coordinate(
                input->pattern_origin.x + x,
                LIFE_WIDTH
            );
            int world_y = life_wrap_coordinate(
                input->pattern_origin.y + y,
                LIFE_HEIGHT
            );

            if (input->terminal_board->cells[world_y + 1][world_x + 1])
                explained[world_y][world_x] = 1;
        }
    }
}

static int outcome_mark_output_gliders(
    uint8_t explained[LIFE_HEIGHT][LIFE_WIDTH],
    const OutcomeAnalysis *analysis)
{
    for (size_t output_index = 0;
         output_index < analysis->output_glider_count;
         ++output_index)
    {
        for (int cell_index = 0;
             cell_index < GLIDER_CELL_COUNT;
             ++cell_index)
        {
            WorldAnchor cell;

            if (!glider_get_live_cell(
                    analysis->output_gliders[output_index],
                    cell_index,
                    &cell))
                return 0;

            int x = life_wrap_coordinate(cell.x, LIFE_WIDTH);
            int y = life_wrap_coordinate(cell.y, LIFE_HEIGHT);
            explained[y][x] = 1;
        }
    }

    return 1;
}

static int outcome_count_debris(
    const OutcomeAnalysisInput *input,
    OutcomeAnalysis *analysis)
{
    uint8_t explained[LIFE_HEIGHT][LIFE_WIDTH];
    memset(explained, 0, sizeof(explained));
    outcome_mark_pattern_cells(explained, input);

    if (!outcome_mark_output_gliders(explained, analysis)) return 0;

    for (int y = 0; y < LIFE_HEIGHT; ++y)
    {
        for (int x = 0; x < LIFE_WIDTH; ++x)
        {
            if (input->terminal_board->cells[y + 1][x + 1] &&
                !explained[y][x])
                analysis->debris_cell_count += 1;
        }
    }

    analysis->debris_present = analysis->debris_cell_count > 0
        ? COLLISION_VALUE_YES
        : COLLISION_VALUE_NO;
    return 1;
}

static void outcome_find_surviving_input(
    const OutcomeAnalysisInput *input,
    OutcomeAnalysis *analysis)
{
    analysis->incoming_glider_survived = COLLISION_VALUE_NO;

    for (size_t i = 0; i < analysis->output_glider_count; ++i)
    {
        if (glider_geometry_matches(
                analysis->output_gliders[i],
                input->expected_incoming_glider))
        {
            analysis->incoming_glider_survived = COLLISION_VALUE_YES;
            return;
        }
    }
}

static CollisionOutcome outcome_classify(
    const OutcomeAnalysisInput *input,
    const OutcomeAnalysis *analysis)
{
    if (input->timed_out ||
        analysis->pattern_stable != COLLISION_VALUE_YES)
        return COLLISION_OUTCOME_TIMEOUT;

    if (analysis->pattern_state == COLLISION_PATTERN_DESTROYED)
        return COLLISION_OUTCOME_PATTERN_DESTROYED;

    if (analysis->pattern_state == COLLISION_PATTERN_CHANGED)
        return COLLISION_OUTCOME_PATTERN_CHANGED;

    if (analysis->output_glider_count > 1)
        return COLLISION_OUTCOME_MULTI_GLIDER_OUTPUT;

    if (analysis->output_glider_count == 1)
    {
        if (analysis->incoming_glider_survived == COLLISION_VALUE_YES)
            return COLLISION_OUTCOME_PASS_THROUGH;

        if (analysis->output_gliders[0].direction !=
            input->incoming_glider.direction)
            return COLLISION_OUTCOME_REFLECT;

        return COLLISION_OUTCOME_GLIDER_OUTPUT;
    }

    if (analysis->debris_present == COLLISION_VALUE_YES)
        return COLLISION_OUTCOME_DEBRIS;

    return COLLISION_OUTCOME_ABSORB;
}

int outcome_analyze(
    const OutcomeAnalysisInput *input,
    OutcomeAnalysis *analysis)
{
    if (!input || !analysis || !input->terminal_board ||
        !input->pattern ||
        !glider_signal_is_valid(input->incoming_glider) ||
        !glider_signal_is_valid(input->expected_incoming_glider))
        return 0;

    memset(analysis, 0, sizeof(*analysis));
    analysis->outcome = COLLISION_OUTCOME_UNKNOWN;
    analysis->pattern_state = outcome_pattern_state(
        input->terminal_board,
        input->pattern,
        input->pattern_origin
    );
    analysis->pattern_stable = outcome_pattern_is_stable(input)
        ? COLLISION_VALUE_YES
        : COLLISION_VALUE_NO;
    analysis->incoming_glider_survived = COLLISION_VALUE_UNKNOWN;
    analysis->debris_present = COLLISION_VALUE_UNKNOWN;

    if (!outcome_find_output_gliders(input, analysis) ||
        !outcome_count_debris(input, analysis))
        return 0;

    outcome_find_surviving_input(input, analysis);
    analysis->outcome = outcome_classify(input, analysis);
    return 1;
}

const char *collision_outcome_name(CollisionOutcome outcome)
{
    switch (outcome)
    {
        case COLLISION_OUTCOME_ABSORB:
            return "ABSORB";
        case COLLISION_OUTCOME_PASS_THROUGH:
            return "PASS_THROUGH";
        case COLLISION_OUTCOME_REFLECT:
            return "REFLECT";
        case COLLISION_OUTCOME_GLIDER_OUTPUT:
            return "GLIDER_OUTPUT";
        case COLLISION_OUTCOME_MULTI_GLIDER_OUTPUT:
            return "MULTI_GLIDER_OUTPUT";
        case COLLISION_OUTCOME_PATTERN_CHANGED:
            return "PATTERN_CHANGED";
        case COLLISION_OUTCOME_PATTERN_DESTROYED:
            return "PATTERN_DESTROYED";
        case COLLISION_OUTCOME_DEBRIS:
            return "DEBRIS";
        case COLLISION_OUTCOME_TIMEOUT:
            return "TIMEOUT";
        case COLLISION_OUTCOME_FAILED:
            return "FAILED";
        case COLLISION_OUTCOME_UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

const char *collision_pattern_state_name(CollisionPatternState state)
{
    switch (state)
    {
        case COLLISION_PATTERN_RESTORED:
            return "PATTERN_RESTORED";
        case COLLISION_PATTERN_CHANGED:
            return "PATTERN_CHANGED";
        case COLLISION_PATTERN_DESTROYED:
            return "PATTERN_DESTROYED";
        case COLLISION_PATTERN_UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

const char *collision_value_name(CollisionValue value)
{
    switch (value)
    {
        case COLLISION_VALUE_YES:
            return "yes";
        case COLLISION_VALUE_NO:
            return "no";
        case COLLISION_VALUE_UNKNOWN:
        default:
            return "unknown";
    }
}
