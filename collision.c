#include "collision.h"

#include <stdio.h>
#include <string.h>

#define COLLISION_APPROACH_GENERATIONS 64
#define COLLISION_STABILITY_MARGIN_GENERATIONS 16

static int collision_box_fits_board(
    WorldAnchor origin,
    WorldBox relative_box)
{
    return origin.x + relative_box.min_x >= 0 &&
           origin.y + relative_box.min_y >= 0 &&
           origin.x + relative_box.max_x < LIFE_WIDTH &&
           origin.y + relative_box.max_y < LIFE_HEIGHT;
}

static SignalState collision_contract_input(
    const CollisionScenario *scenario,
    const Eater1Contract *contract,
    uint64_t generation)
{
    SignalState input = contract->input_signal;
    input.anchor.x += scenario->pattern_origin.x;
    input.anchor.y += scenario->pattern_origin.y;
    input.generation = generation;
    return input;
}

static int collision_prepare_reaction_recording(CollisionRun *run)
{
    WorldBox relative_bbox = run->eater1_contract.reaction_bbox;
    relative_bbox.min_x -= COLLISION_REACTION_CONTEXT_MARGIN;
    relative_bbox.min_y -= COLLISION_REACTION_CONTEXT_MARGIN;
    relative_bbox.max_x += COLLISION_REACTION_CONTEXT_MARGIN;
    relative_bbox.max_y += COLLISION_REACTION_CONTEXT_MARGIN;

    if (!collision_box_fits_board(
            run->scenario.pattern_origin,
            relative_bbox))
        return 0;

    ReactionRecording *recording = &run->result.reaction;
    recording->bbox = (WorldBox)
    {
        run->scenario.pattern_origin.x + relative_bbox.min_x,
        run->scenario.pattern_origin.y + relative_bbox.min_y,
        run->scenario.pattern_origin.x + relative_bbox.max_x,
        run->scenario.pattern_origin.y + relative_bbox.max_y
    };
    recording->width = (int)(
        recording->bbox.max_x - recording->bbox.min_x + 1
    );
    recording->height = (int)(
        recording->bbox.max_y - recording->bbox.min_y + 1
    );
    recording->last_recorded_generation = UINT64_MAX;

    return recording->width <= COLLISION_REACTION_MAX_WIDTH &&
           recording->height <= COLLISION_REACTION_MAX_HEIGHT;
}

static void collision_timeline_include_board(CollisionRun *run)
{
    const LifeBoard *board = collision_run_board(run);
    ReactionTimeline *timeline = &run->result.timeline;

    for (int y = 0; y < LIFE_HEIGHT; ++y)
    {
        for (int x = 0; x < LIFE_WIDTH; ++x)
        {
            if (!board->cells[y + 1][x + 1]) continue;

            if (!timeline->has_reaction_bbox)
            {
                timeline->reaction_bbox = (WorldBox){x, y, x, y};
                timeline->has_reaction_bbox = 1;
                continue;
            }

            if (x < timeline->reaction_bbox.min_x)
                timeline->reaction_bbox.min_x = x;
            if (x > timeline->reaction_bbox.max_x)
                timeline->reaction_bbox.max_x = x;
            if (y < timeline->reaction_bbox.min_y)
                timeline->reaction_bbox.min_y = y;
            if (y > timeline->reaction_bbox.max_y)
                timeline->reaction_bbox.max_y = y;
        }
    }
}

static int collision_record_reaction_frame(CollisionRun *run)
{
    ReactionRecording *recording = &run->result.reaction;

    if (recording->last_recorded_generation == run->generation)
        return 1;

    if (recording->frame_count >= COLLISION_REACTION_MAX_FRAMES)
        return 0;

    size_t frame_index = recording->frame_count;
    const LifeBoard *board = collision_run_board(run);

    for (int y = 0; y < recording->height; ++y)
    {
        for (int x = 0; x < recording->width; ++x)
        {
            recording->cells[frame_index][y][x] =
                life_cell_is_alive(
                    board,
                    recording->bbox.min_x + x,
                    recording->bbox.min_y + y
                );
        }
    }

    if (recording->frame_count == 0)
        recording->first_generation = run->generation;

    recording->last_generation = run->generation;
    recording->last_recorded_generation = run->generation;
    recording->frame_count += 1;
    return 1;
}

int collision_build_eater1_scenario(CollisionScenario *scenario)
{
    if (!scenario) return 0;

    Eater1Contract contract;

    if (!eater1_get_contract(&contract)) return 0;

    WorldAnchor origin = {LIFE_WIDTH / 2, LIFE_HEIGHT / 2};

    if (!collision_box_fits_board(origin, contract.safety_bbox))
        return 0;

    SignalState incoming = contract.input_signal;
    incoming.anchor.x += origin.x;
    incoming.anchor.y += origin.y;
    incoming.generation = COLLISION_APPROACH_GENERATIONS;

    for (int generation = 0;
         generation < COLLISION_APPROACH_GENERATIONS;
         ++generation)
        incoming = glider_rewind(incoming);

    if (incoming.generation != 0) return 0;

    scenario->pattern = &EATER1_PATTERN;
    scenario->pattern_origin = origin;
    scenario->incoming_glider = incoming;
    scenario->simulation_limit =
        COLLISION_APPROACH_GENERATIONS +
        contract.restore_generation_offset +
        COLLISION_STABILITY_MARGIN_GENERATIONS;

    return 1;
}

static int collision_find_input_generation(
    const CollisionScenario *scenario,
    const Eater1Contract *contract,
    uint64_t *input_generation)
{
    SignalState probe = scenario->incoming_glider;

    for (;;)
    {
        SignalState expected_input = collision_contract_input(
            scenario,
            contract,
            probe.generation
        );

        if (glider_geometry_matches(probe, expected_input))
        {
            *input_generation = probe.generation;
            return 1;
        }

        if (probe.generation >= scenario->simulation_limit) break;

        probe = glider_advance(probe);
    }

    return 0;
}

int collision_run_initialize(
    CollisionRun *run,
    const CollisionScenario *scenario)
{
    if (!run) return 0;

    memset(run, 0, sizeof(*run));
    run->state = COLLISION_RUN_FAILED;

    if (!scenario ||
        scenario->pattern != &EATER1_PATTERN ||
        scenario->incoming_glider.generation != 0 ||
        scenario->incoming_glider.direction < SE ||
        scenario->incoming_glider.direction >= DIRECTION_COUNT ||
        scenario->incoming_glider.phase < P0 ||
        scenario->incoming_glider.phase >= PHASE_COUNT ||
        !eater1_get_contract(&run->eater1_contract) ||
        !collision_box_fits_board(
            scenario->pattern_origin,
            run->eater1_contract.safety_bbox) ||
        !collision_find_input_generation(
            scenario,
            &run->eater1_contract,
            &run->scheduled_input_generation))
        return 0;

    if (run->scheduled_input_generation +
            run->eater1_contract.restore_generation_offset >
        scenario->simulation_limit)
        return 0;

    life_world_reset(&run->world);
    LifeBoard *board = life_world_current(&run->world);
    pattern_place_generation0(
        board,
        scenario->pattern,
        scenario->pattern_origin
    );

    if (!glider_place_generation0(
            board,
            scenario->incoming_glider) ||
        life_live_cell_count(board) !=
            scenario->pattern->live_cell_count + GLIDER_CELL_COUNT)
        return 0;

    run->scenario = *scenario;
    run->generation = 0;
    run->reaction_start_generation =
        run->scheduled_input_generation +
        run->eater1_contract.reaction_start_offset;
    run->result.outcome = COLLISION_OUTCOME_UNKNOWN;
    run->result.pattern_state = COLLISION_PATTERN_UNKNOWN;
    run->result.pattern_stable = COLLISION_VALUE_UNKNOWN;
    run->result.pattern_restored = COLLISION_VALUE_UNKNOWN;
    run->result.pattern_changed = COLLISION_VALUE_UNKNOWN;
    run->result.incoming_glider_survived =
        COLLISION_VALUE_UNKNOWN;
    run->result.debris_present = COLLISION_VALUE_UNKNOWN;
    run->result.initial_live_cells = life_live_cell_count(board);
    run->result.timeline.initial_generation = 0;

    if (!collision_prepare_reaction_recording(run)) return 0;

    run->phase = COLLISION_PHASE_APPROACH;
    run->state = COLLISION_RUN_RUNNING;
    run->verification_result = VERIFY_OK;
    glider_tracker_initialize(
        &run->tracker,
        scenario->incoming_glider
    );

    return 1;
}

static CollisionRunState collision_fail(
    CollisionRun *run,
    VerificationResult result)
{
    run->verification_result = result;
    run->state = COLLISION_RUN_FAILED;
    return run->state;
}

static CollisionRunState collision_observe_approach(
    CollisionRun *run)
{
    int entering_reaction =
        run->generation == run->scheduled_input_generation;

    run->verification_result = glider_tracker_observe(
        &run->tracker,
        collision_run_board(run),
        run->generation,
        !entering_reaction
    );

    if (run->verification_result != VERIFY_OK)
        return collision_fail(run, run->verification_result);

    if (!pattern_matches_at(
            collision_run_board(run),
            run->scenario.pattern,
            run->scenario.pattern_origin))
        return collision_fail(run, VERIFY_EATER_PATTERN_MISMATCH);

    if (!entering_reaction) return run->state;

    SignalState expected_input = collision_contract_input(
        &run->scenario,
        &run->eater1_contract,
        run->scheduled_input_generation
    );

    if (!glider_states_equal(
            run->tracker.expected,
            expected_input))
        return collision_fail(
            run,
            VERIFY_COMPONENT_CONTRACT_MISMATCH
        );

    run->phase = COLLISION_PHASE_REACTION;
    return run->state;
}

static CollisionRunState collision_observe_reaction(
    CollisionRun *run)
{
    if (run->generation < run->reaction_start_generation)
        return run->state;

    if (!run->result.timeline.has_reaction_start)
    {
        run->result.timeline.first_interaction_generation =
            run->generation;
        run->result.timeline.has_first_interaction = 1;
        run->result.timeline.reaction_start_generation =
            run->generation;
        run->result.timeline.has_reaction_start = 1;
    }

    collision_timeline_include_board(run);

    if (!collision_record_reaction_frame(run))
        return collision_fail(
            run,
            VERIFY_RECORDING_CAPACITY_EXCEEDED
        );

    size_t frame_index = (size_t)(
        run->generation - run->reaction_start_generation
    );

    run->verification_result = eater1_verify_reaction_frame(
        collision_run_board(run),
        &run->eater1_contract,
        run->scenario.pattern_origin,
        frame_index
    );

    if (run->verification_result != VERIFY_OK)
        return collision_fail(run, run->verification_result);

    if (frame_index + 1 ==
        run->eater1_contract.reaction_frames.frame_count)
    {
        if (!pattern_board_equals_only(
                collision_run_board(run),
                run->scenario.pattern,
                run->scenario.pattern_origin))
            return collision_fail(
                run,
                VERIFY_EATER_PATTERN_MISMATCH
            );

        run->state = COLLISION_RUN_ABSORBED;
    }

    return run->state;
}

static SignalState collision_expected_input_at_generation(
    const CollisionScenario *scenario,
    uint64_t generation)
{
    SignalState expected = scenario->incoming_glider;

    while (expected.generation < generation)
        expected = glider_advance(expected);

    return expected;
}

static void collision_finalize_result(CollisionRun *run)
{
    if (!run || run->result_ready ||
        run->state == COLLISION_RUN_RUNNING)
        return;

    CollisionResult *result = &run->result;
    const LifeBoard *board = collision_run_board(run);

    result->timeline.terminal_generation = run->generation;
    result->timeline.has_terminal = 1;
    result->generation_count =
        run->generation - result->timeline.initial_generation;
    result->reaction_duration =
        result->timeline.has_reaction_start
            ? run->generation -
                result->timeline.reaction_start_generation
            : 0;
    result->final_live_cells = life_live_cell_count(board);

    OutcomeAnalysisInput analysis_input =
    {
        board,
        run->scenario.pattern,
        run->scenario.pattern_origin,
        run->scenario.incoming_glider,
        collision_expected_input_at_generation(
            &run->scenario,
            run->generation
        ),
        run->generation,
        result->reaction.bbox,
        run->state == COLLISION_RUN_TIMEOUT
    };
    OutcomeAnalysis analysis;

    if (!outcome_analyze(&analysis_input, &analysis))
    {
        result->outcome = COLLISION_OUTCOME_FAILED;
        run->result_ready = 1;
        return;
    }

    result->outcome = run->state == COLLISION_RUN_FAILED
        ? COLLISION_OUTCOME_FAILED
        : analysis.outcome;
    result->pattern_state = analysis.pattern_state;
    result->pattern_stable = analysis.pattern_stable;
    result->pattern_restored =
        analysis.pattern_state == COLLISION_PATTERN_RESTORED
            ? COLLISION_VALUE_YES
            : COLLISION_VALUE_NO;
    result->pattern_changed =
        analysis.pattern_state == COLLISION_PATTERN_CHANGED ||
        analysis.pattern_state == COLLISION_PATTERN_DESTROYED
            ? COLLISION_VALUE_YES
            : COLLISION_VALUE_NO;
    result->incoming_glider_survived =
        analysis.incoming_glider_survived;
    result->debris_present = analysis.debris_present;
    result->debris_cell_count = analysis.debris_cell_count;
    result->output_glider_count = analysis.output_glider_count;

    for (size_t i = 0; i < analysis.output_glider_count; ++i)
        result->output_gliders[i] = analysis.output_gliders[i];

    run->result_ready = 1;
}

CollisionRunState collision_run_observe(CollisionRun *run)
{
    if (!run || run->state != COLLISION_RUN_RUNNING)
        return run ? run->state : COLLISION_RUN_FAILED;

    CollisionRunState state =
        run->phase == COLLISION_PHASE_APPROACH
            ? collision_observe_approach(run)
            : collision_observe_reaction(run);

    if (state == COLLISION_RUN_RUNNING &&
        run->generation >= run->scenario.simulation_limit)
        run->state = COLLISION_RUN_TIMEOUT;

    collision_finalize_result(run);

    return run->state;
}

int collision_run_step(CollisionRun *run)
{
    if (!run || run->state != COLLISION_RUN_RUNNING)
        return 0;

    if (run->generation >= run->scenario.simulation_limit)
    {
        run->state = COLLISION_RUN_TIMEOUT;
        collision_finalize_result(run);
        return 0;
    }

    life_world_step(&run->world);
    run->generation += 1;
    return 1;
}

CollisionRunState collision_run_headless(
    const CollisionScenario *scenario,
    CollisionRun *run)
{
    if (!collision_run_initialize(run, scenario))
        return COLLISION_RUN_FAILED;

    while (collision_run_observe(run) == COLLISION_RUN_RUNNING)
        collision_run_step(run);

    return run->state;
}

const LifeBoard *collision_run_board(const CollisionRun *run)
{
    if (!run) return NULL;

    return life_world_current_const(&run->world);
}

const CollisionResult *collision_run_result(const CollisionRun *run)
{
    if (!run || !run->result_ready) return NULL;

    return &run->result;
}

void collision_result_print(const CollisionResult *result)
{
    if (!result) return;

    printf("Collision result:\n");
    printf("outcome=%s\n", collision_outcome_name(result->outcome));
    printf(
        "initial_generation=%llu\n",
        (unsigned long long)result->timeline.initial_generation
    );

    if (result->timeline.has_first_interaction)
        printf(
            "first_interaction=%llu\n",
            (unsigned long long)
                result->timeline.first_interaction_generation
        );
    else
        printf("first_interaction=unknown\n");

    if (result->timeline.has_reaction_start)
        printf(
            "reaction_start=%llu\n",
            (unsigned long long)
                result->timeline.reaction_start_generation
        );
    else
        printf("reaction_start=unknown\n");

    if (result->timeline.has_terminal)
        printf(
            "terminal_generation=%llu\n",
            (unsigned long long)result->timeline.terminal_generation
        );
    else
        printf("terminal_generation=unknown\n");

    printf(
        "generation_count=%llu\n",
        (unsigned long long)result->generation_count
    );
    printf(
        "reaction_duration=%llu\n",
        (unsigned long long)result->reaction_duration
    );
    printf(
        "pattern_state=%s\n",
        collision_pattern_state_name(result->pattern_state)
    );
    printf(
        "pattern_stable=%s\n",
        collision_value_name(result->pattern_stable)
    );
    printf(
        "pattern_restored=%s\n",
        collision_value_name(result->pattern_restored)
    );
    printf(
        "pattern_changed=%s\n",
        collision_value_name(result->pattern_changed)
    );
    printf(
        "incoming_glider_survived=%s\n",
        collision_value_name(result->incoming_glider_survived)
    );
    printf(
        "debris=%s\n",
        collision_value_name(result->debris_present)
    );
    printf("debris_cells=%zu\n", result->debris_cell_count);
    printf("output_gliders=%zu\n", result->output_glider_count);

    for (size_t i = 0; i < result->output_glider_count; ++i)
    {
        const SignalState *output = &result->output_gliders[i];
        printf(
            "output[%zu]=anchor=(%lld,%lld) direction=%s "
            "phase=%s generation=%llu\n",
            i,
            (long long)output->anchor.x,
            (long long)output->anchor.y,
            glider_direction_name(output->direction),
            glider_phase_name(output->phase),
            (unsigned long long)output->generation
        );
    }

    printf("initial_live_cells=%d\n", result->initial_live_cells);
    printf("final_live_cells=%d\n", result->final_live_cells);
    printf("recorded_frames=%zu\n", result->reaction.frame_count);

    if (result->timeline.has_reaction_bbox)
        printf(
            "reaction_bbox=(%lld,%lld)-(%lld,%lld)\n",
            (long long)result->timeline.reaction_bbox.min_x,
            (long long)result->timeline.reaction_bbox.min_y,
            (long long)result->timeline.reaction_bbox.max_x,
            (long long)result->timeline.reaction_bbox.max_y
        );
    else
        printf("reaction_bbox=unknown\n");
}

const char *collision_run_state_name(CollisionRunState state)
{
    switch (state)
    {
        case COLLISION_RUN_RUNNING:
            return "RUNNING";
        case COLLISION_RUN_ABSORBED:
            return "ABSORBED";
        case COLLISION_RUN_TIMEOUT:
            return "TIMEOUT";
        case COLLISION_RUN_FAILED:
        default:
            return "FAILED";
    }
}
