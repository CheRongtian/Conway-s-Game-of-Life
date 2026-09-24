#include "collision_search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    SEARCH_INPUT_VALID,
    SEARCH_INPUT_INVALID_INITIAL_STATE,
    SEARCH_INPUT_NO_APPROACH
} SearchInputStatus;

typedef struct
{
    size_t cell_count;
    WorldAnchor *cells;
} SearchFrame;

typedef struct
{
    size_t frame_count;
    size_t frame_capacity;
    SearchFrame *frames;
} SearchSignature;

typedef struct
{
    CollisionSearchCase summary;
    SearchSignature signature;
} StoredSearchCase;

typedef struct
{
    OutcomeAnalysis analysis;
    uint64_t reaction_start_generation;
    uint64_t terminal_generation;
    WorldBox reaction_bbox;
    int has_reaction_bbox;
    SearchSignature signature;
} SearchExecution;

static int search_signal_overlaps_pattern(
    SignalState signal,
    const PatternSpec *pattern,
    WorldAnchor pattern_origin)
{
    for (int glider_index = 0;
         glider_index < GLIDER_CELL_COUNT;
         ++glider_index)
    {
        WorldAnchor glider_cell;

        if (!glider_get_live_cell(
                signal,
                glider_index,
                &glider_cell))
            return 1;

        for (int pattern_index = 0;
             pattern_index < pattern->live_cell_count;
             ++pattern_index)
        {
            WorldAnchor pattern_cell =
            {
                pattern_origin.x + pattern->cells[pattern_index].x,
                pattern_origin.y + pattern->cells[pattern_index].y
            };

            if (glider_cell.x == pattern_cell.x &&
                glider_cell.y == pattern_cell.y)
                return 1;
        }
    }

    return 0;
}

static int search_signal_touches_pattern(
    SignalState signal,
    const PatternSpec *pattern,
    WorldAnchor pattern_origin)
{
    if (!glider_signal_is_valid(signal)) return 1;

    for (int pattern_index = 0;
         pattern_index < pattern->live_cell_count;
         ++pattern_index)
    {
        int64_t relative_x =
            pattern_origin.x + pattern->cells[pattern_index].x -
            signal.anchor.x;
        int64_t relative_y =
            pattern_origin.y + pattern->cells[pattern_index].y -
            signal.anchor.y;

        if (relative_x >= -1 &&
            relative_x <= GLIDER_PATTERN_SIZE &&
            relative_y >= -1 &&
            relative_y <= GLIDER_PATTERN_SIZE)
            return 1;
    }

    return 0;
}

static SearchInputStatus search_filter_scenario(
    const CollisionScenario *scenario)
{
    if (!scenario || !scenario->pattern ||
        scenario->incoming_glider.generation != 0 ||
        !glider_signal_is_valid(scenario->incoming_glider))
        return SEARCH_INPUT_INVALID_INITIAL_STATE;

    if (search_signal_overlaps_pattern(
            scenario->incoming_glider,
            scenario->pattern,
            scenario->pattern_origin) ||
        search_signal_touches_pattern(
            scenario->incoming_glider,
            scenario->pattern,
            scenario->pattern_origin))
        return SEARCH_INPUT_INVALID_INITIAL_STATE;

    SignalState probe = scenario->incoming_glider;

    for (uint64_t generation = 1;
         generation <= scenario->simulation_limit;
         ++generation)
    {
        probe = glider_advance(probe);

        if (search_signal_touches_pattern(
                probe,
                scenario->pattern,
                scenario->pattern_origin))
            return SEARCH_INPUT_VALID;
    }

    return SEARCH_INPUT_NO_APPROACH;
}

static int search_boards_equal(
    const LifeBoard *first,
    const LifeBoard *second)
{
    if (!first || !second) return 0;

    for (int y = 1; y <= LIFE_HEIGHT; ++y)
    {
        if (memcmp(
                &first->cells[y][1],
                &second->cells[y][1],
                LIFE_WIDTH * sizeof(first->cells[y][1])) != 0)
            return 0;
    }

    return 1;
}

static WorldBox search_reaction_region(
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    return (WorldBox)
    {
        origin.x - OUTCOME_REACTION_MARGIN,
        origin.y - OUTCOME_REACTION_MARGIN,
        origin.x + pattern->width - 1 + OUTCOME_REACTION_MARGIN,
        origin.y + pattern->height - 1 + OUTCOME_REACTION_MARGIN
    };
}

static int search_region_equal(
    const LifeBoard *first,
    const LifeBoard *second,
    WorldBox region)
{
    for (int64_t y = region.min_y; y <= region.max_y; ++y)
    {
        for (int64_t x = region.min_x; x <= region.max_x; ++x)
        {
            if (life_cell_is_alive(first, x, y) !=
                life_cell_is_alive(second, x, y))
                return 0;
        }
    }

    return 1;
}

static int search_signal_outside_region(
    SignalState signal,
    WorldBox region)
{
    int64_t max_x = signal.anchor.x + GLIDER_PATTERN_SIZE - 1;
    int64_t max_y = signal.anchor.y + GLIDER_PATTERN_SIZE - 1;

    return max_x < region.min_x || signal.anchor.x > region.max_x ||
           max_y < region.min_y || signal.anchor.y > region.max_y;
}

static int search_signature_prepare(
    SearchSignature *signature,
    size_t frame_capacity)
{
    memset(signature, 0, sizeof(*signature));
    signature->frames = calloc(frame_capacity, sizeof(*signature->frames));

    if (!signature->frames) return 0;

    signature->frame_capacity = frame_capacity;
    return 1;
}

static void search_signature_destroy(SearchSignature *signature)
{
    if (!signature) return;

    for (size_t i = 0; i < signature->frame_count; ++i)
        free(signature->frames[i].cells);

    free(signature->frames);
    memset(signature, 0, sizeof(*signature));
}

static int search_signature_append(
    SearchExecution *execution,
    const LifeBoard *board,
    const LifeBoard *pattern_reference,
    WorldAnchor pattern_origin)
{
    SearchSignature *signature = &execution->signature;

    if (!signature || !board ||
        signature->frame_count >= signature->frame_capacity)
        return 0;

    SearchFrame *frame = &signature->frames[signature->frame_count];
    frame->cell_count = (size_t)life_live_cell_count(board);

    if (frame->cell_count > 0)
    {
        frame->cells = malloc(frame->cell_count * sizeof(*frame->cells));

        if (!frame->cells) return 0;
    }

    size_t cell_index = 0;

    for (int y = 0; y < LIFE_HEIGHT; ++y)
    {
        for (int x = 0; x < LIFE_WIDTH; ++x)
        {
            if (board->cells[y + 1][x + 1] !=
                pattern_reference->cells[y + 1][x + 1])
            {
                if (!execution->has_reaction_bbox)
                {
                    execution->reaction_bbox = (WorldBox){x, y, x, y};
                    execution->has_reaction_bbox = 1;
                }
                else
                {
                    if (x < execution->reaction_bbox.min_x)
                        execution->reaction_bbox.min_x = x;
                    if (x > execution->reaction_bbox.max_x)
                        execution->reaction_bbox.max_x = x;
                    if (y < execution->reaction_bbox.min_y)
                        execution->reaction_bbox.min_y = y;
                    if (y > execution->reaction_bbox.max_y)
                        execution->reaction_bbox.max_y = y;
                }
            }

            if (!board->cells[y + 1][x + 1]) continue;

            frame->cells[cell_index] = (WorldAnchor)
            {
                x - pattern_origin.x,
                y - pattern_origin.y
            };
            cell_index += 1;
        }
    }

    signature->frame_count += 1;
    return 1;
}

static int search_frame_equal(
    const SearchFrame *first,
    const SearchFrame *second)
{
    if (first->cell_count != second->cell_count) return 0;

    for (size_t i = 0; i < first->cell_count; ++i)
    {
        if (first->cells[i].x != second->cells[i].x ||
            first->cells[i].y != second->cells[i].y)
            return 0;
    }

    return 1;
}

static int search_signature_equal(
    const SearchSignature *first,
    const SearchSignature *second)
{
    if (first->frame_count != second->frame_count) return 0;

    for (size_t i = 0; i < first->frame_count; ++i)
    {
        if (!search_frame_equal(&first->frames[i], &second->frames[i]))
            return 0;
    }

    return 1;
}

static int search_terminal_state_equal(
    const SearchSignature *first,
    const SearchSignature *second)
{
    if (first->frame_count == 0 || second->frame_count == 0)
        return 0;

    return search_frame_equal(
        &first->frames[first->frame_count - 1],
        &second->frames[second->frame_count - 1]
    );
}

static int search_analyze_terminal(
    const CollisionScenario *scenario,
    const LifeBoard *board,
    SignalState expected,
    uint64_t generation,
    WorldBox reaction_region,
    int timed_out,
    SearchExecution *execution)
{
    OutcomeAnalysisInput input =
    {
        board,
        scenario->pattern,
        scenario->pattern_origin,
        scenario->incoming_glider,
        expected,
        generation,
        reaction_region,
        timed_out
    };

    if (!execution->has_reaction_bbox)
    {
        execution->reaction_bbox = (WorldBox)
        {
            scenario->pattern_origin.x,
            scenario->pattern_origin.y,
            scenario->pattern_origin.x + scenario->pattern->width - 1,
            scenario->pattern_origin.y + scenario->pattern->height - 1
        };
        execution->has_reaction_bbox = 1;
    }

    execution->terminal_generation = generation;
    return outcome_analyze(&input, &execution->analysis);
}

static int search_simulate_scenario(
    const CollisionScenario *scenario,
    SearchExecution *execution)
{
    memset(execution, 0, sizeof(*execution));

    if (!search_signature_prepare(
            &execution->signature,
            (size_t)scenario->simulation_limit + 1))
        return 0;

    LifeWorld world;
    life_world_reset(&world);
    LifeBoard *initial_board = life_world_current(&world);
    LifeBoard pattern_reference;
    life_board_clear(&pattern_reference);
    pattern_place_generation0(
        initial_board,
        scenario->pattern,
        scenario->pattern_origin
    );
    pattern_place_generation0(
        &pattern_reference,
        scenario->pattern,
        scenario->pattern_origin
    );

    if (!glider_place_generation0(
            initial_board,
            scenario->incoming_glider) ||
        life_live_cell_count(initial_board) !=
            scenario->pattern->live_cell_count + GLIDER_CELL_COUNT)
    {
        search_signature_destroy(&execution->signature);
        return 0;
    }

    SignalState expected = scenario->incoming_glider;
    WorldBox reaction_region = search_reaction_region(
        scenario->pattern,
        scenario->pattern_origin
    );
    int interaction_started = 0;
    int stable_from_previous_generation = 0;
    int reaction_region_stable_generations = 0;

    for (uint64_t generation = 0;
         generation <= scenario->simulation_limit;
         ++generation)
    {
        const LifeBoard *board = life_world_current_const(&world);
        VerificationResult glider_result = glider_verify(
            board,
            expected,
            generation
        );
        int pattern_matches = pattern_matches_at(
            board,
            scenario->pattern,
            scenario->pattern_origin
        );

        if (!interaction_started &&
            (glider_result != VERIFY_OK || !pattern_matches))
        {
            interaction_started = 1;
            execution->reaction_start_generation = generation;
            reaction_region_stable_generations = 0;
        }

        if (interaction_started &&
            !search_signature_append(
                execution,
                board,
                &pattern_reference,
                scenario->pattern_origin))
        {
            search_signature_destroy(&execution->signature);
            return 0;
        }

        int absorbed = interaction_started &&
            pattern_board_equals_only(
                board,
                scenario->pattern,
                scenario->pattern_origin
            );
        int input_escaped = interaction_started &&
            glider_result == VERIFY_OK &&
            pattern_matches &&
            life_live_cell_count(board) ==
                scenario->pattern->live_cell_count + GLIDER_CELL_COUNT &&
            search_signal_outside_region(expected, reaction_region);
        int terminal = absorbed || input_escaped ||
            (interaction_started && stable_from_previous_generation) ||
            reaction_region_stable_generations >=
                OUTCOME_STABILITY_GENERATIONS;

        if (terminal)
        {
            if (!search_analyze_terminal(
                    scenario,
                    board,
                    expected,
                    generation,
                    reaction_region,
                    0,
                    execution))
            {
                search_signature_destroy(&execution->signature);
                return 0;
            }

            return 1;
        }

        if (generation == scenario->simulation_limit)
        {
            if (!search_analyze_terminal(
                    scenario,
                    board,
                    expected,
                    generation,
                    reaction_region,
                    1,
                    execution))
            {
                search_signature_destroy(&execution->signature);
                return 0;
            }

            return 1;
        }

        const LifeBoard *previous = board;
        life_world_step(&world);
        const LifeBoard *current = life_world_current_const(&world);
        stable_from_previous_generation = search_boards_equal(
            previous,
            current
        );

        if (interaction_started &&
            search_region_equal(previous, current, reaction_region))
            reaction_region_stable_generations += 1;
        else
            reaction_region_stable_generations = 0;

        expected = glider_advance(expected);
    }

    search_signature_destroy(&execution->signature);
    return 0;
}

static int search_inputs_equal(
    const CollisionSearchCase *first,
    const CollisionSearchCase *second)
{
    return first->direction == second->direction &&
           first->phase == second->phase &&
           first->offset_x == second->offset_x &&
           first->offset_y == second->offset_y;
}

static int search_cases_are_duplicates(
    const StoredSearchCase *stored,
    const CollisionSearchCase *candidate,
    const SearchSignature *signature)
{
    if (search_inputs_equal(&stored->summary, candidate)) return 1;

    if (stored->summary.outcome != candidate->outcome) return 0;

    if (search_signature_equal(&stored->signature, signature)) return 1;

    return search_terminal_state_equal(&stored->signature, signature);
}

static int search_store_case(
    StoredSearchCase **stored_cases,
    size_t *stored_count,
    size_t *stored_capacity,
    CollisionSearchCase summary,
    SearchSignature *signature,
    CollisionSearchReport *report)
{
    for (size_t i = 0; i < *stored_count; ++i)
    {
        if (!search_cases_are_duplicates(
                &(*stored_cases)[i],
                &summary,
                signature))
            continue;

        (*stored_cases)[i].summary.occurrences += 1;
        report->duplicate_cases += 1;
        search_signature_destroy(signature);
        return 1;
    }

    if (*stored_count == *stored_capacity)
    {
        size_t new_capacity = *stored_capacity == 0
            ? 16
            : *stored_capacity * 2;
        StoredSearchCase *resized = realloc(
            *stored_cases,
            new_capacity * sizeof(**stored_cases)
        );

        if (!resized) return 0;

        *stored_cases = resized;
        *stored_capacity = new_capacity;
    }

    summary.occurrences = 1;
    (*stored_cases)[*stored_count].summary = summary;
    (*stored_cases)[*stored_count].signature = *signature;
    memset(signature, 0, sizeof(*signature));
    *stored_count += 1;
    return 1;
}

static void search_destroy_stored_cases(
    StoredSearchCase *stored_cases,
    size_t stored_count)
{
    for (size_t i = 0; i < stored_count; ++i)
        search_signature_destroy(&stored_cases[i].signature);

    free(stored_cases);
}

static void search_count_outcome(
    CollisionSearchReport *report,
    const OutcomeAnalysis *analysis)
{
    if (analysis->outcome == COLLISION_OUTCOME_TIMEOUT)
        report->timeout += 1;
    else if (analysis->outcome != COLLISION_OUTCOME_UNKNOWN &&
             analysis->outcome != COLLISION_OUTCOME_FAILED)
        report->completed += 1;

    if (analysis->incoming_glider_survived == COLLISION_VALUE_YES)
        report->survived += 1;

    if (analysis->debris_present == COLLISION_VALUE_YES)
        report->debris_present += 1;

    switch (analysis->outcome)
    {
        case COLLISION_OUTCOME_ABSORB:
            report->absorbed += 1;
            break;
        case COLLISION_OUTCOME_PASS_THROUGH:
            report->pass_through += 1;
            break;
        case COLLISION_OUTCOME_REFLECT:
            report->reflected += 1;
            break;
        case COLLISION_OUTCOME_GLIDER_OUTPUT:
            report->glider_output += 1;
            break;
        case COLLISION_OUTCOME_MULTI_GLIDER_OUTPUT:
            report->multi_glider_output += 1;
            break;
        case COLLISION_OUTCOME_PATTERN_CHANGED:
            report->pattern_changed += 1;
            break;
        case COLLISION_OUTCOME_PATTERN_DESTROYED:
            report->pattern_destroyed += 1;
            break;
        case COLLISION_OUTCOME_DEBRIS:
            report->debris += 1;
            break;
        case COLLISION_OUTCOME_TIMEOUT:
        case COLLISION_OUTCOME_UNKNOWN:
        case COLLISION_OUTCOME_FAILED:
        default:
            break;
    }
}

int collision_search_eater1(CollisionSearchReport *report)
{
    if (!report) return 0;

    memset(report, 0, sizeof(*report));
    StoredSearchCase *stored_cases = NULL;
    size_t stored_count = 0;
    size_t stored_capacity = 0;
    WorldAnchor origin = {LIFE_WIDTH / 2, LIFE_HEIGHT / 2};

    for (int direction = SE; direction < DIRECTION_COUNT; ++direction)
    {
        for (int phase = P0; phase < PHASE_COUNT; ++phase)
        {
            for (int offset_x = -COLLISION_SEARCH_RADIUS;
                 offset_x <= COLLISION_SEARCH_RADIUS;
                 ++offset_x)
            {
                for (int offset_y = -COLLISION_SEARCH_RADIUS;
                     offset_y <= COLLISION_SEARCH_RADIUS;
                     ++offset_y)
                {
                    CollisionScenario scenario =
                    {
                        &EATER1_PATTERN,
                        origin,
                        {
                            {origin.x + offset_x, origin.y + offset_y},
                            (Direction)direction,
                            (GliderPhase)phase,
                            0
                        },
                        COLLISION_SEARCH_SIMULATION_LIMIT
                    };
                    report->tested += 1;

                    SearchInputStatus status =
                        search_filter_scenario(&scenario);

                    if (status == SEARCH_INPUT_INVALID_INITIAL_STATE)
                    {
                        report->invalid_initial_state += 1;
                        continue;
                    }

                    if (status == SEARCH_INPUT_NO_APPROACH)
                    {
                        report->filtered_no_approach += 1;
                        continue;
                    }

                    report->valid += 1;
                    SearchExecution execution;

                    if (!search_simulate_scenario(&scenario, &execution))
                    {
                        search_destroy_stored_cases(
                            stored_cases,
                            stored_count
                        );
                        collision_search_report_destroy(report);
                        return 0;
                    }

                    search_count_outcome(report, &execution.analysis);
                    CollisionSearchCase summary =
                    {
                        .direction = (Direction)direction,
                        .phase = (GliderPhase)phase,
                        .offset_x = offset_x,
                        .offset_y = offset_y,
                        .outcome = execution.analysis.outcome,
                        .pattern_state = execution.analysis.pattern_state,
                        .pattern_stable = execution.analysis.pattern_stable,
                        .incoming_glider_survived =
                            execution.analysis.incoming_glider_survived,
                        .debris_present =
                            execution.analysis.debris_present,
                        .debris_cell_count =
                            execution.analysis.debris_cell_count,
                        .output_glider_count =
                            execution.analysis.output_glider_count,
                        .reaction_start_generation =
                            execution.reaction_start_generation,
                        .reaction_duration =
                            execution.terminal_generation -
                            execution.reaction_start_generation,
                        .terminal_generation =
                            execution.terminal_generation,
                        .reaction_bbox = execution.reaction_bbox,
                        .has_reaction_bbox =
                            execution.has_reaction_bbox,
                        .occurrences = 1
                    };

                    for (size_t i = 0;
                         i < execution.analysis.output_glider_count;
                         ++i)
                        summary.output_gliders[i] =
                            execution.analysis.output_gliders[i];

                    if (!search_store_case(
                            &stored_cases,
                            &stored_count,
                            &stored_capacity,
                            summary,
                            &execution.signature,
                            report))
                    {
                        search_signature_destroy(&execution.signature);
                        search_destroy_stored_cases(
                            stored_cases,
                            stored_count
                        );
                        collision_search_report_destroy(report);
                        return 0;
                    }
                }
            }
        }
    }

    if (stored_count > 0)
    {
        report->cases = malloc(stored_count * sizeof(*report->cases));

        if (!report->cases)
        {
            search_destroy_stored_cases(stored_cases, stored_count);
            collision_search_report_destroy(report);
            return 0;
        }

        for (size_t i = 0; i < stored_count; ++i)
            report->cases[i] = stored_cases[i].summary;
    }

    report->case_count = stored_count;
    search_destroy_stored_cases(stored_cases, stored_count);
    return 1;
}

int collision_search_case_scenario(
    const CollisionSearchCase *search_case,
    CollisionScenario *scenario)
{
    if (!search_case || !scenario ||
        search_case->direction < SE ||
        search_case->direction >= DIRECTION_COUNT ||
        search_case->phase < P0 ||
        search_case->phase >= PHASE_COUNT)
        return 0;

    WorldAnchor origin = {LIFE_WIDTH / 2, LIFE_HEIGHT / 2};
    *scenario = (CollisionScenario)
    {
        &EATER1_PATTERN,
        origin,
        {
            {
                origin.x + search_case->offset_x,
                origin.y + search_case->offset_y
            },
            search_case->direction,
            search_case->phase,
            0
        },
        COLLISION_SEARCH_SIMULATION_LIMIT
    };
    return 1;
}

void collision_search_report_print(const CollisionSearchReport *report)
{
    if (!report) return;

    printf("Collision search:\n");
    printf("tested=%zu\n", report->tested);
    printf("valid=%zu\n", report->valid);
    printf(
        "invalid_initial_state=%zu\n",
        report->invalid_initial_state
    );
    printf("filtered_no_approach=%zu\n", report->filtered_no_approach);
    printf("completed=%zu\n", report->completed);
    printf("ABSORB=%zu\n", report->absorbed);
    printf("PASS_THROUGH=%zu\n", report->pass_through);
    printf("REFLECT=%zu\n", report->reflected);
    printf("GLIDER_OUTPUT=%zu\n", report->glider_output);
    printf("MULTI_GLIDER_OUTPUT=%zu\n", report->multi_glider_output);
    printf("PATTERN_CHANGED=%zu\n", report->pattern_changed);
    printf("PATTERN_DESTROYED=%zu\n", report->pattern_destroyed);
    printf("DEBRIS=%zu\n", report->debris);
    printf("TIMEOUT=%zu\n", report->timeout);
    printf("incoming_glider_survived=%zu\n", report->survived);
    printf("debris_present=%zu\n", report->debris_present);
    printf("duplicates=%zu\n", report->duplicate_cases);
    printf("meaningful_cases=%zu\n", report->case_count);

    for (size_t i = 0; i < report->case_count; ++i)
    {
        const CollisionSearchCase *search_case = &report->cases[i];
        printf(
            "direction=%s phase=%s offset=(%d,%d) "
            "result=%s pattern=%s stable=%s outputs=%zu "
            "incoming_survived=%s debris=%s debris_cells=%zu "
            "reaction_duration=%llu terminal_generation=%llu "
            "occurrences=%zu\n",
            glider_direction_name(search_case->direction),
            glider_phase_name(search_case->phase),
            search_case->offset_x,
            search_case->offset_y,
            collision_outcome_name(search_case->outcome),
            collision_pattern_state_name(search_case->pattern_state),
            collision_value_name(search_case->pattern_stable),
            search_case->output_glider_count,
            collision_value_name(
                search_case->incoming_glider_survived
            ),
            collision_value_name(search_case->debris_present),
            search_case->debris_cell_count,
            (unsigned long long)search_case->reaction_duration,
            (unsigned long long)search_case->terminal_generation,
            search_case->occurrences
        );

        for (size_t output_index = 0;
             output_index < search_case->output_glider_count;
             ++output_index)
        {
            const SignalState *output =
                &search_case->output_gliders[output_index];
            printf(
                "  output[%zu]=anchor=(%lld,%lld) direction=%s "
                "phase=%s generation=%llu\n",
                output_index,
                (long long)output->anchor.x,
                (long long)output->anchor.y,
                glider_direction_name(output->direction),
                glider_phase_name(output->phase),
                (unsigned long long)output->generation
            );
        }
    }
}

void collision_search_report_destroy(CollisionSearchReport *report)
{
    if (!report) return;

    free(report->cases);
    memset(report, 0, sizeof(*report));
}
