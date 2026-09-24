#include "eater1.h"

#include <stdint.h>
#include <string.h>

#define EATER1_DISCOVERY_RADIUS 20
#define EATER1_DISCOVERY_GENERATIONS 64
#define EATER1_STABILITY_GENERATIONS 16
#define EATER1_MIN_SAFETY_MARGIN 4

typedef struct
{
    int found;
    SignalState initial_signal;
    SignalState entry_signal;
    uint64_t entry_generation;
    uint64_t reaction_start_generation;
    uint64_t restore_generation;
    int initial_distance;
} Eater1Candidate;

static const CellOffset eater1_cells[EATER1_CELL_COUNT] =
{
    {0, 0}, {1, 0},
    {0, 1}, {2, 1},
    {2, 2},
    {2, 3}, {3, 3}
};

const PatternSpec EATER1_PATTERN =
{
    EATER1_PATTERN_ID,
    EATER1_WIDTH,
    EATER1_HEIGHT,
    EATER1_CELL_COUNT,
    eater1_cells
};

static Eater1Contract canonical_contract;
static int canonical_contract_ready;

static int glider_overlaps_eater1(
    SignalState signal,
    WorldAnchor origin)
{
    LifeBoard glider_board;
    life_board_clear(&glider_board);

    if (signal.generation != 0 ||
        !glider_place_generation0(&glider_board, signal))
        return 1;

    for (int i = 0; i < EATER1_CELL_COUNT; ++i)
    {
        if (life_cell_is_alive(
                &glider_board,
                origin.x + eater1_cells[i].x,
                origin.y + eater1_cells[i].y))
            return 1;
    }

    return 0;
}

static int eater1_enters_glider_isolation_box(
    SignalState signal,
    WorldAnchor origin)
{
    for (int i = 0; i < EATER1_CELL_COUNT; ++i)
    {
        int64_t relative_x =
            origin.x + eater1_cells[i].x - signal.anchor.x;
        int64_t relative_y =
            origin.y + eater1_cells[i].y - signal.anchor.y;

        if (relative_x >= -1 &&
            relative_x <= GLIDER_PATTERN_SIZE &&
            relative_y >= -1 &&
            relative_y <= GLIDER_PATTERN_SIZE)
            return 1;
    }

    return 0;
}

static int eater1_is_entry_candidate(
    SignalState signal,
    WorldAnchor origin)
{
    if (eater1_enters_glider_isolation_box(signal, origin))
        return 0;

    return eater1_enters_glider_isolation_box(
        glider_advance(signal),
        origin
    );
}

static int eater1_simulate_candidate(
    WorldAnchor origin,
    SignalState initial_signal,
    Eater1Candidate *candidate)
{
    if (!candidate || glider_overlaps_eater1(initial_signal, origin))
        return 0;

    LifeWorld world;
    life_world_reset(&world);
    pattern_place_generation0(
        life_world_current(&world),
        &EATER1_PATTERN,
        origin
    );

    if (!glider_place_generation0(
            life_world_current(&world),
            initial_signal))
        return 0;

    SignalState expected = initial_signal;
    uint64_t last_isolated_generation =
        GLIDER_GENERATION_UNOBSERVED;
    SignalState last_isolated_signal = initial_signal;
    uint64_t reaction_start_generation =
        GLIDER_GENERATION_UNOBSERVED;

    for (uint64_t generation = 0;
         generation <= EATER1_DISCOVERY_GENERATIONS;
         ++generation)
    {
        const LifeBoard *board = life_world_current_const(&world);

        if (reaction_start_generation ==
            GLIDER_GENERATION_UNOBSERVED)
        {
            VerificationResult result = glider_verify(
                board,
                expected,
                generation
            );

            if (result == VERIFY_OK)
            {
                last_isolated_generation = generation;
                last_isolated_signal = expected;
            }
            else
            {
                if (last_isolated_generation ==
                    GLIDER_GENERATION_UNOBSERVED)
                    return 0;

                reaction_start_generation = generation;
            }
        }

        if (reaction_start_generation !=
                GLIDER_GENERATION_UNOBSERVED &&
            pattern_board_equals_only(
                board,
                &EATER1_PATTERN,
                origin))
        {
            for (int stable_generation = 0;
                 stable_generation < EATER1_STABILITY_GENERATIONS;
                 ++stable_generation)
            {
                life_world_step(&world);

                if (!pattern_board_equals_only(
                        life_world_current_const(&world),
                        &EATER1_PATTERN,
                        origin))
                    return 0;
            }

            candidate->found = 1;
            candidate->initial_signal = initial_signal;
            candidate->entry_signal = last_isolated_signal;
            candidate->entry_generation = last_isolated_generation;
            candidate->reaction_start_generation =
                reaction_start_generation;
            candidate->restore_generation = generation;
            return 1;
        }

        if (generation == EATER1_DISCOVERY_GENERATIONS) break;

        life_world_step(&world);
        expected = glider_advance(expected);
    }

    return 0;
}

static int eater1_candidate_is_better(
    const Eater1Candidate *candidate,
    const Eater1Candidate *best)
{
    if (!best->found) return 1;

    if (candidate->reaction_start_generation !=
        best->reaction_start_generation)
        return candidate->reaction_start_generation <
               best->reaction_start_generation;

    if (candidate->initial_distance != best->initial_distance)
        return candidate->initial_distance < best->initial_distance;

    uint64_t candidate_duration =
        candidate->restore_generation -
        candidate->reaction_start_generation;
    uint64_t best_duration =
        best->restore_generation -
        best->reaction_start_generation;

    return candidate_duration < best_duration;
}

static void eater1_update_box_from_board(
    const LifeBoard *board,
    WorldBox *box,
    int *has_cells)
{
    for (int y = 0; y < LIFE_HEIGHT; ++y)
    {
        for (int x = 0; x < LIFE_WIDTH; ++x)
        {
            if (!board->cells[y + 1][x + 1]) continue;

            if (!*has_cells)
            {
                box->min_x = x;
                box->max_x = x;
                box->min_y = y;
                box->max_y = y;
                *has_cells = 1;
                continue;
            }

            if (x < box->min_x) box->min_x = x;
            if (x > box->max_x) box->max_x = x;
            if (y < box->min_y) box->min_y = y;
            if (y > box->max_y) box->max_y = y;
        }
    }
}

static int eater1_replay_candidate(
    WorldAnchor origin,
    const Eater1Candidate *candidate,
    Eater1Contract *contract)
{
    LifeWorld world;
    life_world_reset(&world);
    pattern_place_generation0(
        life_world_current(&world),
        &EATER1_PATTERN,
        origin
    );

    if (!glider_place_generation0(
            life_world_current(&world),
            candidate->initial_signal))
        return 0;

    WorldBox absolute_reaction_box = {0, 0, 0, 0};
    int has_reaction_cells = 0;

    for (uint64_t generation = 0;
         generation <= candidate->restore_generation;
         ++generation)
    {
        if (generation >= candidate->reaction_start_generation)
            eater1_update_box_from_board(
                life_world_current_const(&world),
                &absolute_reaction_box,
                &has_reaction_cells
            );

        if (generation == candidate->restore_generation) break;

        life_world_step(&world);
    }

    if (!has_reaction_cells ||
        !pattern_board_equals_only(
            life_world_current_const(&world),
            &EATER1_PATTERN,
            origin))
        return 0;

    int reaction_width =
        (int)(absolute_reaction_box.max_x -
              absolute_reaction_box.min_x + 1);
    int reaction_height =
        (int)(absolute_reaction_box.max_y -
              absolute_reaction_box.min_y + 1);
    uint64_t frame_count =
        candidate->restore_generation -
        candidate->reaction_start_generation + 1;

    if (reaction_width > EATER1_REACTION_MAX_WIDTH ||
        reaction_height > EATER1_REACTION_MAX_HEIGHT ||
        frame_count > EATER1_REACTION_MAX_FRAMES)
        return 0;

    memset(contract, 0, sizeof(*contract));
    contract->input_signal = candidate->entry_signal;
    contract->input_signal.anchor.x -= origin.x;
    contract->input_signal.anchor.y -= origin.y;
    contract->input_signal.generation = 0;
    contract->reaction_start_offset =
        candidate->reaction_start_generation -
        candidate->entry_generation;
    contract->restore_generation_offset =
        candidate->restore_generation -
        candidate->entry_generation;
    contract->reaction_duration =
        candidate->restore_generation -
        candidate->reaction_start_generation;
    contract->pattern_bbox = (WorldBox)
    {
        0,
        0,
        EATER1_WIDTH - 1,
        EATER1_HEIGHT - 1
    };
    contract->reaction_bbox = (WorldBox)
    {
        absolute_reaction_box.min_x - origin.x,
        absolute_reaction_box.min_y - origin.y,
        absolute_reaction_box.max_x - origin.x,
        absolute_reaction_box.max_y - origin.y
    };

    int64_t safety_margin =
        contract->reaction_duration > EATER1_MIN_SAFETY_MARGIN
            ? (int64_t)contract->reaction_duration
            : EATER1_MIN_SAFETY_MARGIN;

    contract->safety_bbox = (WorldBox)
    {
        contract->reaction_bbox.min_x - safety_margin,
        contract->reaction_bbox.min_y - safety_margin,
        contract->reaction_bbox.max_x + safety_margin,
        contract->reaction_bbox.max_y + safety_margin
    };
    contract->reaction_frames.frame_count = (size_t)frame_count;
    contract->reaction_frames.width = reaction_width;
    contract->reaction_frames.height = reaction_height;

    life_world_reset(&world);
    pattern_place_generation0(
        life_world_current(&world),
        &EATER1_PATTERN,
        origin
    );

    if (!glider_place_generation0(
            life_world_current(&world),
            candidate->initial_signal))
        return 0;

    for (uint64_t generation = 0;
         generation <= candidate->restore_generation;
         ++generation)
    {
        if (generation >= candidate->reaction_start_generation)
        {
            size_t frame_index = (size_t)(
                generation - candidate->reaction_start_generation
            );

            for (int y = 0; y < reaction_height; ++y)
            {
                for (int x = 0; x < reaction_width; ++x)
                {
                    contract->reaction_frames.cells
                        [frame_index][y][x] = life_cell_is_alive(
                            life_world_current_const(&world),
                            absolute_reaction_box.min_x + x,
                            absolute_reaction_box.min_y + y
                        );
                }
            }
        }

        if (generation == candidate->restore_generation) break;

        life_world_step(&world);
    }

    return 1;
}

static int eater1_verify_still_life(void)
{
    WorldAnchor origin = {LIFE_WIDTH / 2, LIFE_HEIGHT / 2};
    LifeWorld world;
    life_world_reset(&world);
    pattern_place_generation0(
        life_world_current(&world),
        &EATER1_PATTERN,
        origin
    );

    for (int generation = 0; generation <= 64; ++generation)
    {
        if (!pattern_board_equals_only(
                life_world_current_const(&world),
                &EATER1_PATTERN,
                origin))
            return 0;

        if (generation < 64) life_world_step(&world);
    }

    return 1;
}

static int eater1_discover_contract(Eater1Contract *contract)
{
    WorldAnchor origin = {LIFE_WIDTH / 2, LIFE_HEIGHT / 2};
    Eater1Candidate best = {0};

    for (int direction = 0; direction < DIRECTION_COUNT; ++direction)
    {
        for (int phase = 0; phase < PHASE_COUNT; ++phase)
        {
            for (int dy = -EATER1_DISCOVERY_RADIUS;
                 dy <= EATER1_DISCOVERY_RADIUS;
                 ++dy)
            {
                for (int dx = -EATER1_DISCOVERY_RADIUS;
                     dx <= EATER1_DISCOVERY_RADIUS;
                     ++dx)
                {
                    SignalState initial =
                    {
                        {origin.x + dx, origin.y + dy},
                        (Direction)direction,
                        (GliderPhase)phase,
                        0
                    };
                    Eater1Candidate candidate = {0};

                    if (!eater1_is_entry_candidate(initial, origin) ||
                        !eater1_simulate_candidate(
                            origin,
                            initial,
                            &candidate))
                        continue;

                    int absolute_dx = dx < 0 ? -dx : dx;
                    int absolute_dy = dy < 0 ? -dy : dy;
                    candidate.initial_distance =
                        absolute_dx + absolute_dy;

                    if (eater1_candidate_is_better(&candidate, &best))
                        best = candidate;
                }
            }
        }
    }

    if (!best.found ||
        best.reaction_start_generation != best.entry_generation + 1)
        return 0;

    return eater1_replay_candidate(origin, &best, contract);
}

int eater1_get_contract(Eater1Contract *contract)
{
    if (!contract) return 0;

    if (!canonical_contract_ready)
    {
        if (!eater1_verify_still_life() ||
            !eater1_discover_contract(&canonical_contract))
            return 0;

        canonical_contract_ready = 1;
    }

    *contract = canonical_contract;
    return 1;
}

VerificationResult eater1_verify_reaction_frame(
    const LifeBoard *board,
    const Eater1Contract *contract,
    WorldAnchor origin,
    size_t frame_index)
{
    if (!board || !contract ||
        frame_index >= contract->reaction_frames.frame_count)
        return VERIFY_REACTION_FRAME_MISMATCH;

    for (int64_t relative_y = contract->safety_bbox.min_y;
         relative_y <= contract->safety_bbox.max_y;
         ++relative_y)
    {
        for (int64_t relative_x = contract->safety_bbox.min_x;
             relative_x <= contract->safety_bbox.max_x;
             ++relative_x)
        {
            int inside_reaction =
                relative_x >= contract->reaction_bbox.min_x &&
                relative_x <= contract->reaction_bbox.max_x &&
                relative_y >= contract->reaction_bbox.min_y &&
                relative_y <= contract->reaction_bbox.max_y;
            int expected = 0;

            if (inside_reaction)
            {
                int frame_x = (int)(
                    relative_x - contract->reaction_bbox.min_x
                );
                int frame_y = (int)(
                    relative_y - contract->reaction_bbox.min_y
                );
                expected = contract->reaction_frames.cells
                    [frame_index][frame_y][frame_x] != 0;
            }

            int actual = life_cell_is_alive(
                board,
                origin.x + relative_x,
                origin.y + relative_y
            );

            if (actual != expected)
                return inside_reaction
                    ? VERIFY_REACTION_FRAME_MISMATCH
                    : VERIFY_COMPONENT_SAFETY_VIOLATION;
        }
    }

    return VERIFY_OK;
}
