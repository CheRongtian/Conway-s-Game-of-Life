#include "collision.h"

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

    return run->state;
}

int collision_run_step(CollisionRun *run)
{
    if (!run || run->state != COLLISION_RUN_RUNNING)
        return 0;

    if (run->generation >= run->scenario.simulation_limit)
    {
        run->state = COLLISION_RUN_TIMEOUT;
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
