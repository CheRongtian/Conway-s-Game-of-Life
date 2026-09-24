// From revised1.c
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <SDL.h>

#define WIDTH 512
#define HEIGHT 256

#define CELL_SIZE 2
#define DELAY_MS 10

#define CANVAS_WIDTH 128
#define CANVAS_HEIGHT 48
#define CANVAS_PITCH 2

#define CANVAS_X0 ((WIDTH - CANVAS_WIDTH * CANVAS_PITCH) / 2)
#define CANVAS_Y0 ((HEIGHT - CANVAS_HEIGHT * CANVAS_PITCH) / 2)

#define GLYPH_WIDTH 5
#define GLYPH_HEIGHT 7
#define GLYPH_ADVANCE (GLYPH_WIDTH + 1)
#define MAX_TEXT_LENGTH ((CANVAS_WIDTH + 1) / GLYPH_ADVANCE)
#define INPUT_BUFFER_SIZE (CANVAS_WIDTH + 2)
#define WINDOW_TITLE_SIZE 256

#define GLIDER_CELL_COUNT 5
#define GLIDER_PATTERN_SIZE 3
#define PHASE_2A_TEST_GENERATIONS 16
#define PHASE_2B_TEST_GENERATIONS 32
#define PHASE_3_TEST_GENERATIONS 64
#define MAX_DRAW_EVENTS 4096
#define MAX_STRAIGHT_ROUTE_EVENTS CANVAS_WIDTH
#define STRAIGHT_ROUTE_APPROACH_EVENTS 4
#define STRAIGHT_ROUTE_MAX_GENERATIONS 4096
#define PHASE_4_WRAP_TEST_GENERATIONS 1024
#define EATER1_PATTERN_ID 1
#define EATER1_CELL_COUNT 7
#define EATER1_WIDTH 4
#define EATER1_HEIGHT 4
#define EATER_DISCOVERY_RADIUS 20
#define EATER_DISCOVERY_GENERATIONS 64
#define EATER_STABILITY_GENERATIONS 16
#define EATER_REACTION_MAX_FRAMES 65
#define EATER_REACTION_MAX_WIDTH 64
#define EATER_REACTION_MAX_HEIGHT 64
#define EATER_MIN_SAFETY_MARGIN 4
#define EATER_DEMO_POST_CANVAS_GENERATIONS 32
#define COMPONENT_ID_NONE -1
#define TRACKER_GENERATION_UNPROCESSED UINT64_MAX

#ifndef PHASE_2A_TEST_MODE
#define PHASE_2A_TEST_MODE 0
#endif

#ifndef PHASE_2B_TEST_MODE
#define PHASE_2B_TEST_MODE 0
#endif

#ifndef PHASE_3_TEST_MODE
#define PHASE_3_TEST_MODE 0
#endif

#ifndef PHASE_4_TEST_MODE
#define PHASE_4_TEST_MODE 0
#endif

#ifndef PHASE_5A_TEST_MODE
#define PHASE_5A_TEST_MODE 0
#endif

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
    int64_t x;
    int64_t y;
} WorldAnchor;

typedef struct
{
    int x;
    int y;
} CellOffset;

typedef struct
{
    WorldAnchor anchor;
    Direction direction;
    GliderPhase phase;
    uint64_t generation;
} SignalState;

typedef struct
{
    uint64_t generation;
    uint32_t tracker_id;
    int canvas_u;
    int canvas_v;
    WorldAnchor anchor;
    Direction direction;
    GliderPhase phase;
} DrawEvent;

typedef struct
{
    DrawEvent events[MAX_DRAW_EVENTS];
    size_t count;
} DrawEventLog;

typedef struct
{
    uint8_t cells[CANVAS_HEIGHT][CANVAS_WIDTH];
} RouteMask;

typedef struct
{
    SignalState initial_signal;
    WorldAnchor initial_wrapped_anchor;
    Direction direction;
    int entry_u;
    int entry_v;
    int has_entered_canvas;
    int has_left_canvas;
    uint64_t expected_first_event_generation;
    uint64_t expected_last_event_generation;
    DrawEvent predicted_events[MAX_STRAIGHT_ROUTE_EVENTS];
    size_t predicted_event_count;
    RouteMask predicted_mask;
    WorldAnchor entry_anchor;
    WorldAnchor exit_anchor;
    uint64_t flight_generation_count;
} StraightRoute;

typedef struct
{
    int64_t min_x;
    int64_t min_y;
    int64_t max_x;
    int64_t max_y;
} WorldBox;

typedef struct
{
    int pattern_id;
    int width;
    int height;
    int live_cell_count;
    const CellOffset *cells;
} PatternSpec;

typedef struct
{
    size_t frame_count;
    int width;
    int height;
    uint8_t cells
        [EATER_REACTION_MAX_FRAMES]
        [EATER_REACTION_MAX_HEIGHT]
        [EATER_REACTION_MAX_WIDTH];
} ReactionFrameCache;

typedef struct
{
    int pattern_id;
    WorldAnchor origin;
    SignalState canonical_initial_signal;
    SignalState input_signal;
    uint64_t discovery_entry_generation;
    uint64_t reaction_start_generation;
    uint64_t restore_generation;
    uint64_t reaction_start_offset;
    uint64_t reaction_duration;
    uint64_t restore_generation_offset;
    uint64_t recovery_time;
    uint64_t earliest_next_input_offset;
    uint64_t scheduled_input_generation;
    WorldBox pattern_bbox;
    WorldBox reaction_bbox;
    WorldBox safety_bbox;
    int output_exists;
    ReactionFrameCache reaction_frames;
} EaterEndpoint;

typedef struct
{
    int found;
    SignalState initial_signal;
    SignalState entry_signal;
    uint64_t entry_generation;
    uint64_t reaction_start_generation;
    uint64_t restore_generation;
    int initial_distance;
} EaterPortCandidate;

typedef enum
{
    TRACKER_IN_FLIGHT,
    TRACKER_ENTERING_COMPONENT,
    TRACKER_IN_REACTION,
    TRACKER_ABSORBED,
    TRACKER_COMPLETED,
    TRACKER_FAILED
} TrackerState;

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
    VERIFY_COMPONENT_SAFETY_VIOLATION
} VerificationResult;

typedef struct
{
    SignalState expected;
    TrackerState state;
    uint32_t tracker_id;
    int drawing_enabled;
    uint64_t last_processed_generation;
    uint64_t last_draw_generation;
    int last_draw_u;
    int last_draw_v;
    int component_id;
    uint64_t component_input_generation;
    uint64_t reaction_tick;
    uint64_t failure_generation;
    VerificationResult failure_reason;
} GliderTracker;

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

typedef struct
{
    CollisionScenario scenario;
    uint8_t (*current)[WIDTH + 2];
    uint8_t (*next)[WIDTH + 2];
    uint64_t generation;
    GliderTracker tracker;
    StraightRoute route;
    EaterEndpoint eater_endpoint;
    CollisionRunState state;
    VerificationResult verification_result;
} CollisionRun;

static int signal_states_equal(SignalState a, SignalState b);

static uint8_t board_a[HEIGHT + 2][WIDTH + 2];
static uint8_t board_b[HEIGHT + 2][WIDTH + 2];
static uint8_t target[CANVAS_HEIGHT][CANVAS_WIDTH];
static uint8_t trail[CANVAS_HEIGHT][CANVAS_WIDTH];
static char input_text[MAX_TEXT_LENGTH + 1];
static DrawEventLog event_log;
static uint8_t component_board_a[HEIGHT + 2][WIDTH + 2];
static uint8_t component_board_b[HEIGHT + 2][WIDTH + 2];
static EaterEndpoint canonical_eater1_endpoint;
static EaterEndpoint runtime_eater1_endpoint;
static int eater1_contract_ready;

static const CellOffset eater1_cells[EATER1_CELL_COUNT] =
{
    {0, 0}, {1, 0},
    {0, 1}, {2, 1},
    {2, 2},
    {2, 3}, {3, 3}
};

static const PatternSpec eater1_pattern =
{
    EATER1_PATTERN_ID,
    EATER1_WIDTH,
    EATER1_HEIGHT,
    EATER1_CELL_COUNT,
    eater1_cells
};

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

static const CellOffset direction_delta[DIRECTION_COUNT] =
{
    {1, 1},
    {-1, 1},
    {1, -1},
    {-1, -1}
};

static const uint8_t bitmap_font[37][GLYPH_HEIGHT] =
{
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}, // I
    {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}, // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}, // 5
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}, // 9
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // Space
};

static const uint8_t *get_glyph(char character)
{
    if (character >= 'A' && character <= 'Z')
        return bitmap_font[character - 'A'];

    if (character >= '0' && character <= '9')
        return bitmap_font[26 + character - '0'];

    if (character == ' ')
        return bitmap_font[36];

    return NULL;
}

static int read_input_text(void)
{
    char line[INPUT_BUFFER_SIZE];

    for (;;) 
    {
        printf("Enter text (A-Z, 0-9, space; maximum %d characters): ", MAX_TEXT_LENGTH);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) 
        {
            fprintf(stderr, "Unable to read input.\n");
            return 0;
        }

        if (!strchr(line, '\n') && !feof(stdin)) 
        {
            int character;
            while ((character = getchar()) != '\n' && character != EOF) {}
            fprintf(stderr, "Input is too long. Maximum width is %d canvas pixels.\n", CANVAS_WIDTH);
            continue;
        }

        size_t length = strcspn(line, "\r\n");
        line[length] = '\0';

        int supported = 1;

        for (size_t i = 0; i < length; ++i) 
        {
            unsigned char character = (unsigned char)line[i];

            if (character >= 'a' && character <= 'z')
                line[i] = (char)toupper(character);

            character = (unsigned char)line[i];

            if (!((character >= 'A' && character <= 'Z') ||
                  (character >= '0' && character <= '9') ||
                  character == ' ')) 
            {
                fprintf(
                    stderr,
                    "Unsupported character: 0x%02X. Use A-Z, 0-9, or space.\n",
                    (unsigned int)character
                );
                supported = 0;
                break;
            }
        }

        if (!supported) continue;

        size_t text_width = length ? length * GLYPH_ADVANCE - 1 : 0;

        if (text_width > CANVAS_WIDTH) 
        {
            fprintf(
                stderr,
                "Text is too wide: %zu canvas pixels; maximum is %d.\n",
                text_width,
                CANVAS_WIDTH
            );
            continue;
        }

        memcpy(input_text, line, length + 1);
        return 1;
    }
}

static void build_target(void)
{
    memset(target, 0, sizeof(target));

    size_t length = strlen(input_text);
    int text_width = length ? (int)(length * GLYPH_ADVANCE - 1) : 0;
    int text_u0 = (CANVAS_WIDTH - text_width) / 2;
    int text_v0 = (CANVAS_HEIGHT - GLYPH_HEIGHT) / 2;

    for (size_t i = 0; i < length; ++i) 
    {
        const uint8_t *glyph = get_glyph(input_text[i]);

        for (int row = 0; row < GLYPH_HEIGHT; ++row) 
        {
            for (int column = 0; column < GLYPH_WIDTH; ++column) 
            {
                if (glyph[row] & (1u << (GLYPH_WIDTH - 1 - column))) 
                {
                    target[text_v0 + row]
                          [text_u0 + (int)i * GLYPH_ADVANCE + column] = 1;
                }
            }
        }
    }
}

static int wrap_coord(int64_t coordinate, int size)
{
    int64_t wrapped = coordinate % size;

    if (wrapped < 0) wrapped += size;

    return (int)wrapped;
}

static int canvas_to_anchor(int u, int v, WorldAnchor *anchor)
{
    if (u < 0 || u >= CANVAS_WIDTH ||
        v < 0 || v >= CANVAS_HEIGHT)
        return 0;

    anchor->x = CANVAS_X0 + (int64_t)u * CANVAS_PITCH;
    anchor->y = CANVAS_Y0 + (int64_t)v * CANVAS_PITCH;

    return 1;
}

static int anchor_to_canvas(WorldAnchor anchor, int *u, int *v)
{
    int64_t canvas_x1 =
        CANVAS_X0 + (int64_t)(CANVAS_WIDTH - 1) * CANVAS_PITCH;
    int64_t canvas_y1 =
        CANVAS_Y0 + (int64_t)(CANVAS_HEIGHT - 1) * CANVAS_PITCH;

    if (anchor.x < CANVAS_X0 || anchor.x > canvas_x1 ||
        anchor.y < CANVAS_Y0 || anchor.y > canvas_y1)
        return 0;

    int64_t offset_x = anchor.x - CANVAS_X0;
    int64_t offset_y = anchor.y - CANVAS_Y0;

    if (offset_x % CANVAS_PITCH != 0 ||
        offset_y % CANVAS_PITCH != 0)
        return 0;

    *u = (int)(offset_x / CANVAS_PITCH);
    *v = (int)(offset_y / CANVAS_PITCH);

    return 1;
}

static SignalState advance_signal(SignalState signal)
{
    CellOffset delta = anchor_delta[signal.direction][signal.phase];

    signal.anchor.x += delta.x;
    signal.anchor.y += delta.y;
    signal.phase = (GliderPhase)((signal.phase + 1) % PHASE_COUNT);
    signal.generation += 1;

    return signal;
}

static int straight_route_entry_is_valid(
    Direction direction,
    int entry_u,
    int entry_v)
{
    if (entry_u < 0 || entry_u >= CANVAS_WIDTH ||
        entry_v < 0 || entry_v >= CANVAS_HEIGHT)
        return 0;

    switch (direction) 
    {
        case SE:
            return entry_u == 0 || entry_v == 0;
        case SW:
            return entry_u == CANVAS_WIDTH - 1 || entry_v == 0;
        case NE:
            return entry_u == 0 || entry_v == CANVAS_HEIGHT - 1;
        case NW:
            return entry_u == CANVAS_WIDTH - 1 ||
                   entry_v == CANVAS_HEIGHT - 1;
        default:
            return 0;
    }
}

static int straight_route_has_passed_canvas(
    WorldAnchor anchor,
    Direction direction)
{
    int64_t canvas_x1 =
        CANVAS_X0 + (int64_t)(CANVAS_WIDTH - 1) * CANVAS_PITCH;
    int64_t canvas_y1 =
        CANVAS_Y0 + (int64_t)(CANVAS_HEIGHT - 1) * CANVAS_PITCH;

    switch (direction) 
    {
        case SE:
            return anchor.x > canvas_x1 || anchor.y > canvas_y1;
        case SW:
            return anchor.x < CANVAS_X0 || anchor.y > canvas_y1;
        case NE:
            return anchor.x > canvas_x1 || anchor.y < CANVAS_Y0;
        case NW:
            return anchor.x < CANVAS_X0 || anchor.y < CANVAS_Y0;
        default:
            return 0;
    }
}

static int predict_first_canvas_traversal(StraightRoute *route)
{
    SignalState signal = route->initial_signal;
    int entered_canvas = 0;

    route->predicted_event_count = 0;
    route->expected_first_event_generation =
        TRACKER_GENERATION_UNPROCESSED;
    route->expected_last_event_generation =
        TRACKER_GENERATION_UNPROCESSED;
    route->flight_generation_count = 0;
    memset(&route->predicted_mask, 0, sizeof(route->predicted_mask));

    for (int generation = 0;
         generation <= STRAIGHT_ROUTE_MAX_GENERATIONS;
         ++generation) 
    {
        if (signal.phase == P0) 
        {
            int u;
            int v;

            if (anchor_to_canvas(signal.anchor, &u, &v)) 
            {
                if (!entered_canvas) 
                {
                    if (u != route->entry_u || v != route->entry_v)
                        return 0;

                    entered_canvas = 1;
                    route->entry_anchor = signal.anchor;
                    route->expected_first_event_generation =
                        signal.generation;
                }

                if (route->predicted_event_count >=
                    MAX_STRAIGHT_ROUTE_EVENTS)
                    return 0;

                DrawEvent event =
                {
                    signal.generation,
                    0,
                    u,
                    v,
                    signal.anchor,
                    signal.direction,
                    signal.phase
                };

                route->predicted_events
                    [route->predicted_event_count++] = event;
                route->predicted_mask.cells[v][u] ^= 1;
                route->expected_last_event_generation =
                    signal.generation;
            }
            else if (entered_canvas &&
                     straight_route_has_passed_canvas(
                         signal.anchor,
                         route->direction)) 
            {
                route->exit_anchor = signal.anchor;
                route->flight_generation_count = signal.generation;
                return route->predicted_event_count > 0;
            }
        }

        signal = advance_signal(signal);
    }

    return 0;
}

static int plan_straight_route(
    Direction direction,
    int entry_u,
    int entry_v,
    StraightRoute *route)
{
    if (direction < SE || direction >= DIRECTION_COUNT ||
        !straight_route_entry_is_valid(direction, entry_u, entry_v))
        return 0;

    memset(route, 0, sizeof(*route));

    route->direction = direction;
    route->entry_u = entry_u;
    route->entry_v = entry_v;

    WorldAnchor entry_anchor;

    if (!canvas_to_anchor(entry_u, entry_v, &entry_anchor)) return 0;

    SignalState displacement = {{0, 0}, direction, P0, 0};
    int approach_generations = STRAIGHT_ROUTE_APPROACH_EVENTS * 8;

    for (int generation = 0;
         generation < approach_generations;
         ++generation)
        displacement = advance_signal(displacement);

    if (displacement.phase != P0) return 0;

    route->initial_signal.anchor.x =
        entry_anchor.x - displacement.anchor.x;
    route->initial_signal.anchor.y =
        entry_anchor.y - displacement.anchor.y;
    route->initial_signal.direction = direction;
    route->initial_signal.phase = P0;
    route->initial_signal.generation = 0;
    route->initial_wrapped_anchor.x = wrap_coord(
        route->initial_signal.anchor.x,
        WIDTH
    );
    route->initial_wrapped_anchor.y = wrap_coord(
        route->initial_signal.anchor.y,
        HEIGHT
    );
    route->entry_anchor = entry_anchor;

    int initial_u;
    int initial_v;

    if (anchor_to_canvas(
            route->initial_signal.anchor,
            &initial_u,
            &initial_v))
        return 0;

    if (!predict_first_canvas_traversal(route)) return 0;

    if (route->predicted_event_count == 0 ||
        route->predicted_events[0].canvas_u != entry_u ||
        route->predicted_events[0].canvas_v != entry_v ||
        route->expected_first_event_generation !=
            (uint64_t)approach_generations)
        return 0;

    return 1;
}

static int place_glider_generation0(
    uint8_t board[HEIGHT + 2][WIDTH + 2],
    SignalState signal)
{
    if (signal.generation != 0) return 0;

    for (int i = 0; i < GLIDER_CELL_COUNT; ++i) 
    {
        CellOffset offset = glider_offsets
            [signal.direction][signal.phase][i];
        int x = wrap_coord(signal.anchor.x + offset.x, WIDTH);
        int y = wrap_coord(signal.anchor.y + offset.y, HEIGHT);

        board[y + 1][x + 1] = 1;
    }

    return 1;
}

static void initialize_glider_tracker(
    GliderTracker *tracker,
    SignalState signal)
{
    tracker->expected = signal;
    tracker->state = TRACKER_IN_FLIGHT;
    tracker->tracker_id = 0;
    tracker->drawing_enabled = 1;
    tracker->last_processed_generation = TRACKER_GENERATION_UNPROCESSED;
    tracker->last_draw_generation = TRACKER_GENERATION_UNPROCESSED;
    tracker->last_draw_u = -1;
    tracker->last_draw_v = -1;
    tracker->component_id = COMPONENT_ID_NONE;
    tracker->component_input_generation =
        TRACKER_GENERATION_UNPROCESSED;
    tracker->reaction_tick = 0;
    tracker->failure_generation = TRACKER_GENERATION_UNPROCESSED;
    tracker->failure_reason = VERIFY_OK;
}

static int is_expected_glider_cell(
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

static int board_cell_is_alive(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    int64_t world_x,
    int64_t world_y)
{
    int board_x = wrap_coord(world_x, WIDTH);
    int board_y = wrap_coord(world_y, HEIGHT);

    return board[board_y + 1][board_x + 1] != 0;
}

static VerificationResult verify_glider(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    const GliderTracker *tracker,
    uint64_t generation)
{
    if (tracker->state != TRACKER_IN_FLIGHT ||
        tracker->expected.direction < SE ||
        tracker->expected.direction >= DIRECTION_COUNT ||
        tracker->expected.phase < P0 ||
        tracker->expected.phase >= PHASE_COUNT)
        return VERIFY_OUT_OF_EXPECTED_STATE;

    if (generation != tracker->expected.generation)
        return VERIFY_WRONG_GENERATION;

    for (int i = 0; i < GLIDER_CELL_COUNT; ++i) 
    {
        CellOffset offset = glider_offsets
            [tracker->expected.direction][tracker->expected.phase][i];

        if (!board_cell_is_alive(
                board,
                tracker->expected.anchor.x + offset.x,
                tracker->expected.anchor.y + offset.y))
            return VERIFY_MISSING_EXPECTED_LIVE;
    }

    for (int y = 0; y < GLIDER_PATTERN_SIZE; ++y) 
    {
        for (int x = 0; x < GLIDER_PATTERN_SIZE; ++x) 
        {
            if (is_expected_glider_cell(
                    tracker->expected.direction,
                    tracker->expected.phase,
                    x,
                    y))
                continue;

            if (board_cell_is_alive(
                    board,
                    tracker->expected.anchor.x + x,
                    tracker->expected.anchor.y + y))
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

            if (board_cell_is_alive(
                    board,
                    tracker->expected.anchor.x + x,
                    tracker->expected.anchor.y + y))
                return VERIFY_ISOLATION_VIOLATION;
        }
    }

    return VERIFY_OK;
}

static int apply_draw_event(
    uint8_t canvas[CANVAS_HEIGHT][CANVAS_WIDTH],
    const DrawEvent *event)
{
    if (event->canvas_u < 0 || event->canvas_u >= CANVAS_WIDTH ||
        event->canvas_v < 0 || event->canvas_v >= CANVAS_HEIGHT)
        return 0;

    canvas[event->canvas_v][event->canvas_u] ^= 1;

    return 1;
}

static int maybe_emit_draw_event(
    GliderTracker *tracker,
    uint64_t generation,
    DrawEventLog *log,
    uint8_t canvas[CANVAS_HEIGHT][CANVAS_WIDTH])
{
    if (tracker->state != TRACKER_IN_FLIGHT ||
        !tracker->drawing_enabled ||
        tracker->last_processed_generation != generation ||
        tracker->expected.generation != generation ||
        tracker->expected.phase != P0 ||
        tracker->last_draw_generation == generation)
        return 0;

    int u;
    int v;

    if (!anchor_to_canvas(tracker->expected.anchor, &u, &v)) return 0;

    if (log->count >= MAX_DRAW_EVENTS) return 0;

    DrawEvent event =
    {
        generation,
        tracker->tracker_id,
        u,
        v,
        tracker->expected.anchor,
        tracker->expected.direction,
        tracker->expected.phase
    };

    log->events[log->count] = event;

    if (!apply_draw_event(canvas, &event)) return 0;

    log->count += 1;
    tracker->last_draw_generation = generation;
    tracker->last_draw_u = u;
    tracker->last_draw_v = v;

    return 1;
}

static int rebuild_trail_from_event_log(
    uint8_t rebuilt[CANVAS_HEIGHT][CANVAS_WIDTH],
    const DrawEventLog *log)
{
    memset(
        rebuilt,
        0,
        CANVAS_HEIGHT * CANVAS_WIDTH * sizeof(rebuilt[0][0])
    );

    for (size_t i = 0; i < log->count; ++i) 
    {
        if (!apply_draw_event(rebuilt, &log->events[i])) return 0;
    }

    return 1;
}

static void reset_drawing_state(void)
{
    memset(trail, 0, sizeof(trail));
    event_log.count = 0;
}

static int update_straight_route_lifecycle(
    StraightRoute *route,
    GliderTracker *tracker)
{
    if (tracker->expected.phase != P0) return 0;

    int u;
    int v;

    if (anchor_to_canvas(tracker->expected.anchor, &u, &v)) 
    {
        route->has_entered_canvas = 1;
        return 0;
    }

    if (route->has_entered_canvas &&
        straight_route_has_passed_canvas(
            tracker->expected.anchor,
            route->direction)) 
    {
        route->has_left_canvas = 1;
        tracker->drawing_enabled = 0;
        tracker->state = TRACKER_COMPLETED;
        return 1;
    }

    return 0;
}

static void advance_verified_tracker(GliderTracker *tracker)
{
    if (tracker->state != TRACKER_IN_FLIGHT) return;

    tracker->expected = advance_signal(tracker->expected);
}

static void fail_tracker(
    GliderTracker *tracker,
    uint64_t generation,
    VerificationResult reason)
{
    if (tracker->state == TRACKER_FAILED ||
        tracker->state == TRACKER_COMPLETED)
        return;

    tracker->state = TRACKER_FAILED;
    tracker->last_processed_generation = generation;
    tracker->failure_generation = generation;
    tracker->failure_reason = reason;
}

static VerificationResult observe_tracker_internal(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    GliderTracker *tracker,
    StraightRoute *route,
    uint64_t generation)
{
    if (tracker->state == TRACKER_FAILED)
        return tracker->failure_reason;

    if (tracker->state == TRACKER_COMPLETED)
        return VERIFY_OK;

    if (tracker->last_processed_generation == generation)
        return VERIFY_OK;

    if (route && tracker->expected.direction != route->direction) 
    {
        fail_tracker(tracker, generation, VERIFY_OUT_OF_EXPECTED_STATE);
        return VERIFY_OUT_OF_EXPECTED_STATE;
    }

    VerificationResult result = verify_glider(board, tracker, generation);

    if (result != VERIFY_OK) 
    {
        fail_tracker(tracker, generation, result);
        return result;
    }

    tracker->last_processed_generation = generation;

    if (route && update_straight_route_lifecycle(route, tracker))
        return VERIFY_OK;

    maybe_emit_draw_event(tracker, generation, &event_log, trail);
    advance_verified_tracker(tracker);

    return VERIFY_OK;
}

static VerificationResult observe_tracker(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    GliderTracker *tracker,
    uint64_t generation)
{
    return observe_tracker_internal(board, tracker, NULL, generation);
}

static VerificationResult observe_straight_route(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    GliderTracker *tracker,
    StraightRoute *route,
    uint64_t generation)
{
    return observe_tracker_internal(board, tracker, route, generation);
}

static void sync_border(uint8_t board[HEIGHT + 2][WIDTH + 2])
{
    for (int y = 1; y <= HEIGHT; ++y) 
    {
        board[y][0] = board[y][WIDTH];
        board[y][WIDTH + 1] = board[y][1];
    }

    for (int x = 1; x <= WIDTH; ++x) 
    {
        board[0][x] = board[HEIGHT][x];
        board[HEIGHT + 1][x] = board[1][x];
    }

    board[0][0] = board[HEIGHT][WIDTH];
    board[0][WIDTH + 1] = board[HEIGHT][1];
    board[HEIGHT + 1][0] = board[1][WIDTH];
    board[HEIGHT + 1][WIDTH + 1] = board[1][1];
}

static void step(uint8_t current[HEIGHT + 2][WIDTH + 2], uint8_t next[HEIGHT + 2][WIDTH + 2])
{
    sync_border(current);

    for (int y = 1; y <= HEIGHT; ++y) 
    {
        for (int x = 1; x <= WIDTH; ++x) 
        {
            int n =
                current[y - 1][x - 1] +
                current[y - 1][x] +
                current[y - 1][x + 1] +
                current[y][x - 1] +
                current[y][x + 1] +
                current[y + 1][x - 1] +
                current[y + 1][x] +
                current[y + 1][x + 1];

            next[y][x] = (n == 3) || (current[y][x] && n == 2);
        }
    }
}

static int pattern_has_cell(
    const PatternSpec *pattern,
    int x,
    int y)
{
    for (int i = 0; i < pattern->live_cell_count; ++i) 
    {
        if (pattern->cells[i].x == x &&
            pattern->cells[i].y == y)
            return 1;
    }

    return 0;
}

static void place_pattern_generation0(
    uint8_t board[HEIGHT + 2][WIDTH + 2],
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    for (int i = 0; i < pattern->live_cell_count; ++i) 
    {
        int x = wrap_coord(origin.x + pattern->cells[i].x, WIDTH);
        int y = wrap_coord(origin.y + pattern->cells[i].y, HEIGHT);

        board[y + 1][x + 1] = 1;
    }
}

static int pattern_matches_at(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    for (int y = 0; y < pattern->height; ++y) 
    {
        for (int x = 0; x < pattern->width; ++x) 
        {
            int expected = pattern_has_cell(pattern, x, y);
            int actual = board_cell_is_alive(
                board,
                origin.x + x,
                origin.y + y
            );

            if (actual != expected) return 0;
        }
    }

    return 1;
}

static int board_live_cell_count(
    const uint8_t board[HEIGHT + 2][WIDTH + 2])
{
    int count = 0;

    for (int y = 1; y <= HEIGHT; ++y) 
    {
        for (int x = 1; x <= WIDTH; ++x)
            count += board[y][x] != 0;
    }

    return count;
}

static int board_equals_only_pattern(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    return board_live_cell_count(board) == pattern->live_cell_count &&
           pattern_matches_at(board, pattern, origin);
}

static int glider_overlaps_pattern(
    SignalState signal,
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    for (int glider_cell = 0;
         glider_cell < GLIDER_CELL_COUNT;
         ++glider_cell) 
    {
        CellOffset glider_offset = glider_offsets
            [signal.direction][signal.phase][glider_cell];
        int glider_x = wrap_coord(
            signal.anchor.x + glider_offset.x,
            WIDTH
        );
        int glider_y = wrap_coord(
            signal.anchor.y + glider_offset.y,
            HEIGHT
        );

        for (int pattern_cell = 0;
             pattern_cell < pattern->live_cell_count;
             ++pattern_cell) 
        {
            int pattern_x = wrap_coord(
                origin.x + pattern->cells[pattern_cell].x,
                WIDTH
            );
            int pattern_y = wrap_coord(
                origin.y + pattern->cells[pattern_cell].y,
                HEIGHT
            );

            if (glider_x == pattern_x && glider_y == pattern_y)
                return 1;
        }
    }

    return 0;
}

static int pattern_enters_glider_isolation_box(
    SignalState signal,
    const PatternSpec *pattern,
    WorldAnchor origin)
{
    for (int i = 0; i < pattern->live_cell_count; ++i) 
    {
        int64_t relative_x =
            origin.x + pattern->cells[i].x - signal.anchor.x;
        int64_t relative_y =
            origin.y + pattern->cells[i].y - signal.anchor.y;

        if (relative_x >= -1 &&
            relative_x <= GLIDER_PATTERN_SIZE &&
            relative_y >= -1 &&
            relative_y <= GLIDER_PATTERN_SIZE)
            return 1;
    }

    return 0;
}

static int is_eater1_entry_candidate(
    SignalState signal,
    WorldAnchor eater_origin)
{
    if (pattern_enters_glider_isolation_box(
            signal,
            &eater1_pattern,
            eater_origin))
        return 0;

    SignalState next_signal = advance_signal(signal);

    return pattern_enters_glider_isolation_box(
        next_signal,
        &eater1_pattern,
        eater_origin
    );
}

static void update_box_from_board(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    WorldBox *box,
    int *has_cells)
{
    for (int y = 0; y < HEIGHT; ++y) 
    {
        for (int x = 0; x < WIDTH; ++x) 
        {
            if (!board[y + 1][x + 1]) continue;

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

static int simulate_eater1_candidate(
    WorldAnchor eater_origin,
    SignalState initial_signal,
    EaterPortCandidate *candidate)
{
    memset(component_board_a, 0, sizeof(component_board_a));
    memset(component_board_b, 0, sizeof(component_board_b));

    if (glider_overlaps_pattern(
            initial_signal,
            &eater1_pattern,
            eater_origin))
        return 0;

    place_pattern_generation0(
        component_board_a,
        &eater1_pattern,
        eater_origin
    );

    if (!place_glider_generation0(
            component_board_a,
            initial_signal))
        return 0;

    uint8_t (*current)[WIDTH + 2] = component_board_a;
    uint8_t (*next)[WIDTH + 2] = component_board_b;
    SignalState signal = initial_signal;
    GliderTracker verification_tracker;
    initialize_glider_tracker(&verification_tracker, initial_signal);
    uint64_t last_isolated_generation =
        TRACKER_GENERATION_UNPROCESSED;
    SignalState last_isolated_signal = initial_signal;
    uint64_t reaction_start_generation =
        TRACKER_GENERATION_UNPROCESSED;

    for (uint64_t generation = 0;
         generation <= EATER_DISCOVERY_GENERATIONS;
         ++generation) 
    {
        if (reaction_start_generation ==
            TRACKER_GENERATION_UNPROCESSED) 
        {
            verification_tracker.expected = signal;
            verification_tracker.state = TRACKER_IN_FLIGHT;

            VerificationResult result = verify_glider(
                current,
                &verification_tracker,
                generation
            );

            if (result == VERIFY_OK) 
            {
                last_isolated_generation = generation;
                last_isolated_signal = signal;
            }
            else 
            {
                if (last_isolated_generation ==
                    TRACKER_GENERATION_UNPROCESSED)
                    return 0;

                reaction_start_generation = generation;
            }
        }

        if (reaction_start_generation !=
                TRACKER_GENERATION_UNPROCESSED &&
            board_equals_only_pattern(
                current,
                &eater1_pattern,
                eater_origin)) 
        {
            for (int stable_generation = 0;
                 stable_generation < EATER_STABILITY_GENERATIONS;
                 ++stable_generation) 
            {
                step(current, next);

                uint8_t (*tmp)[WIDTH + 2] = current;
                current = next;
                next = tmp;

                if (!board_equals_only_pattern(
                        current,
                        &eater1_pattern,
                        eater_origin))
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

        if (generation == EATER_DISCOVERY_GENERATIONS) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
        signal = advance_signal(signal);
    }

    return 0;
}

static int eater_candidate_is_better(
    const EaterPortCandidate *candidate,
    const EaterPortCandidate *best)
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
        best->restore_generation - best->reaction_start_generation;

    return candidate_duration < best_duration;
}

static int replay_eater1_candidate(
    WorldAnchor eater_origin,
    const EaterPortCandidate *candidate,
    EaterEndpoint *endpoint)
{
    memset(component_board_a, 0, sizeof(component_board_a));
    memset(component_board_b, 0, sizeof(component_board_b));
    place_pattern_generation0(
        component_board_a,
        &eater1_pattern,
        eater_origin
    );

    if (!place_glider_generation0(
            component_board_a,
            candidate->initial_signal))
        return 0;

    uint8_t (*current)[WIDTH + 2] = component_board_a;
    uint8_t (*next)[WIDTH + 2] = component_board_b;
    WorldBox absolute_reaction_box = {0, 0, 0, 0};
    int has_reaction_cells = 0;

    for (uint64_t generation = 0;
         generation <= candidate->restore_generation;
         ++generation) 
    {
        if (generation >= candidate->reaction_start_generation)
            update_box_from_board(
                current,
                &absolute_reaction_box,
                &has_reaction_cells
            );

        if (generation == candidate->restore_generation) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    if (!has_reaction_cells ||
        !board_equals_only_pattern(
            current,
            &eater1_pattern,
            eater_origin))
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

    if (reaction_width > EATER_REACTION_MAX_WIDTH ||
        reaction_height > EATER_REACTION_MAX_HEIGHT ||
        frame_count > EATER_REACTION_MAX_FRAMES)
        return 0;

    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->pattern_id = EATER1_PATTERN_ID;
    endpoint->origin = eater_origin;
    endpoint->canonical_initial_signal = candidate->initial_signal;
    endpoint->canonical_initial_signal.anchor.x -= eater_origin.x;
    endpoint->canonical_initial_signal.anchor.y -= eater_origin.y;
    endpoint->input_signal = candidate->entry_signal;
    endpoint->input_signal.anchor.x -= eater_origin.x;
    endpoint->input_signal.anchor.y -= eater_origin.y;
    endpoint->input_signal.generation = 0;
    endpoint->discovery_entry_generation =
        candidate->entry_generation;
    endpoint->reaction_start_generation =
        candidate->reaction_start_generation;
    endpoint->restore_generation = candidate->restore_generation;
    endpoint->reaction_start_offset =
        candidate->reaction_start_generation -
        candidate->entry_generation;
    endpoint->reaction_duration =
        candidate->restore_generation -
        candidate->reaction_start_generation;
    endpoint->restore_generation_offset =
        candidate->restore_generation -
        candidate->entry_generation;
    endpoint->recovery_time = endpoint->reaction_duration;
    endpoint->earliest_next_input_offset =
        endpoint->restore_generation_offset + 1;
    endpoint->pattern_bbox = (WorldBox)
    {
        0,
        0,
        EATER1_WIDTH - 1,
        EATER1_HEIGHT - 1
    };
    endpoint->reaction_bbox = (WorldBox)
    {
        absolute_reaction_box.min_x - eater_origin.x,
        absolute_reaction_box.min_y - eater_origin.y,
        absolute_reaction_box.max_x - eater_origin.x,
        absolute_reaction_box.max_y - eater_origin.y
    };
    int64_t safety_margin =
        endpoint->reaction_duration > EATER_MIN_SAFETY_MARGIN
            ? (int64_t)endpoint->reaction_duration
            : EATER_MIN_SAFETY_MARGIN;
    endpoint->safety_bbox = (WorldBox)
    {
        endpoint->reaction_bbox.min_x - safety_margin,
        endpoint->reaction_bbox.min_y - safety_margin,
        endpoint->reaction_bbox.max_x + safety_margin,
        endpoint->reaction_bbox.max_y + safety_margin
    };
    endpoint->output_exists = 0;
    endpoint->reaction_frames.frame_count = (size_t)frame_count;
    endpoint->reaction_frames.width = reaction_width;
    endpoint->reaction_frames.height = reaction_height;

    memset(component_board_a, 0, sizeof(component_board_a));
    memset(component_board_b, 0, sizeof(component_board_b));
    place_pattern_generation0(
        component_board_a,
        &eater1_pattern,
        eater_origin
    );

    if (!place_glider_generation0(
            component_board_a,
            candidate->initial_signal))
        return 0;

    current = component_board_a;
    next = component_board_b;

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
                    endpoint->reaction_frames.cells
                        [frame_index][y][x] = board_cell_is_alive(
                            current,
                            absolute_reaction_box.min_x + x,
                            absolute_reaction_box.min_y + y
                        );
                }
            }
        }

        if (generation == candidate->restore_generation) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    return 1;
}

static int discover_eater1_glider_port(EaterEndpoint *endpoint)
{
    WorldAnchor eater_origin = {WIDTH / 2, HEIGHT / 2};
    EaterPortCandidate best = {0};

    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) 
    {
        for (int phase = 0; phase < PHASE_COUNT; ++phase) 
        {
            for (int dy = -EATER_DISCOVERY_RADIUS;
                 dy <= EATER_DISCOVERY_RADIUS;
                 ++dy) 
            {
                for (int dx = -EATER_DISCOVERY_RADIUS;
                     dx <= EATER_DISCOVERY_RADIUS;
                     ++dx) 
                {
                    SignalState initial =
                    {
                        {eater_origin.x + dx, eater_origin.y + dy},
                        (Direction)direction,
                        (GliderPhase)phase,
                        0
                    };
                    EaterPortCandidate candidate = {0};

                    if (!is_eater1_entry_candidate(
                            initial,
                            eater_origin))
                        continue;

                    if (!simulate_eater1_candidate(
                            eater_origin,
                            initial,
                            &candidate))
                        continue;

                    int absolute_dx = dx < 0 ? -dx : dx;
                    int absolute_dy = dy < 0 ? -dy : dy;
                    candidate.initial_distance =
                        absolute_dx + absolute_dy;

                    if (eater_candidate_is_better(
                            &candidate,
                            &best))
                        best = candidate;
                }
            }
        }
    }

    if (!best.found ||
        best.reaction_start_generation != best.entry_generation + 1)
        return 0;

    if (!replay_eater1_candidate(
            eater_origin,
            &best,
            endpoint))
        return 0;

    fprintf(
        stderr,
        "Eater 1 port: direction=%d phase=%d initial_offset=(%lld,%lld) entry=%llu reaction_start=%llu restore=%llu duration=%llu total=%llu.\n",
        endpoint->canonical_initial_signal.direction,
        endpoint->canonical_initial_signal.phase,
        (long long)endpoint->canonical_initial_signal.anchor.x,
        (long long)endpoint->canonical_initial_signal.anchor.y,
        (unsigned long long)endpoint->discovery_entry_generation,
        (unsigned long long)endpoint->reaction_start_generation,
        (unsigned long long)endpoint->restore_generation,
        (unsigned long long)endpoint->reaction_duration,
        (unsigned long long)endpoint->restore_generation_offset
    );

    return 1;
}

static int verify_eater1_still_life(void)
{
    WorldAnchor origin = {WIDTH / 2, HEIGHT / 2};

    memset(component_board_a, 0, sizeof(component_board_a));
    memset(component_board_b, 0, sizeof(component_board_b));
    place_pattern_generation0(
        component_board_a,
        &eater1_pattern,
        origin
    );

    uint8_t (*current)[WIDTH + 2] = component_board_a;
    uint8_t (*next)[WIDTH + 2] = component_board_b;

    for (int generation = 0; generation <= 64; ++generation) 
    {
        if (!board_equals_only_pattern(
                current,
                &eater1_pattern,
                origin))
            return 0;

        if (generation == 64) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    return 1;
}

static int ensure_eater1_contract(void)
{
    if (eater1_contract_ready) return 1;

    if (!verify_eater1_still_life() ||
        !discover_eater1_glider_port(&canonical_eater1_endpoint))
        return 0;

    eater1_contract_ready = 1;
    return 1;
}

static int world_box_fits_board(
    WorldAnchor origin,
    WorldBox relative_box)
{
    return origin.x + relative_box.min_x >= 0 &&
           origin.y + relative_box.min_y >= 0 &&
           origin.x + relative_box.max_x < WIDTH &&
           origin.y + relative_box.max_y < HEIGHT;
}

static int configure_eater1_demo(
    EaterEndpoint *endpoint,
    StraightRoute *route)
{
    if (!ensure_eater1_contract()) return 0;

    *endpoint = canonical_eater1_endpoint;

    int entry_u;
    int entry_v;

    switch (endpoint->input_signal.direction) 
    {
        case SE:
            entry_u = 0;
            entry_v = 0;
            break;
        case SW:
            entry_u = CANVAS_WIDTH - 1;
            entry_v = 0;
            break;
        case NE:
            entry_u = 0;
            entry_v = CANVAS_HEIGHT - 1;
            break;
        case NW:
            entry_u = CANVAS_WIDTH - 1;
            entry_v = CANVAS_HEIGHT - 1;
            break;
        default:
            return 0;
    }

    if (!plan_straight_route(
            endpoint->input_signal.direction,
            entry_u,
            entry_v,
            route))
        return 0;

    SignalState input = route->initial_signal;
    uint64_t earliest_input_generation =
        route->flight_generation_count +
        EATER_DEMO_POST_CANVAS_GENERATIONS;

    while (input.generation < earliest_input_generation ||
           input.phase != endpoint->input_signal.phase)
        input = advance_signal(input);

    endpoint->origin.x =
        input.anchor.x - endpoint->input_signal.anchor.x;
    endpoint->origin.y =
        input.anchor.y - endpoint->input_signal.anchor.y;
    endpoint->scheduled_input_generation = input.generation;

    if (!world_box_fits_board(
            endpoint->origin,
            endpoint->safety_bbox))
        return 0;

    if (glider_overlaps_pattern(
            route->initial_signal,
            &eater1_pattern,
            endpoint->origin))
        return 0;

    return 1;
}

static VerificationResult verify_eater1_reaction_frame(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    const EaterEndpoint *endpoint,
    size_t frame_index)
{
    if (frame_index >= endpoint->reaction_frames.frame_count)
        return VERIFY_REACTION_FRAME_MISMATCH;

    for (int64_t relative_y = endpoint->safety_bbox.min_y;
         relative_y <= endpoint->safety_bbox.max_y;
         ++relative_y) 
    {
        for (int64_t relative_x = endpoint->safety_bbox.min_x;
             relative_x <= endpoint->safety_bbox.max_x;
             ++relative_x) 
        {
            int inside_reaction =
                relative_x >= endpoint->reaction_bbox.min_x &&
                relative_x <= endpoint->reaction_bbox.max_x &&
                relative_y >= endpoint->reaction_bbox.min_y &&
                relative_y <= endpoint->reaction_bbox.max_y;
            int expected = 0;

            if (inside_reaction) 
            {
                int frame_x = (int)(
                    relative_x - endpoint->reaction_bbox.min_x
                );
                int frame_y = (int)(
                    relative_y - endpoint->reaction_bbox.min_y
                );
                expected = endpoint->reaction_frames.cells
                    [frame_index][frame_y][frame_x] != 0;
            }

            int actual = board_cell_is_alive(
                board,
                endpoint->origin.x + relative_x,
                endpoint->origin.y + relative_y
            );

            if (actual != expected)
                return inside_reaction
                    ? VERIFY_REACTION_FRAME_MISMATCH
                    : VERIFY_COMPONENT_SAFETY_VIOLATION;
        }
    }

    return VERIFY_OK;
}

static int signal_matches_eater1_input(
    SignalState signal,
    const EaterEndpoint *endpoint)
{
    return signal.anchor.x ==
               endpoint->origin.x + endpoint->input_signal.anchor.x &&
           signal.anchor.y ==
               endpoint->origin.y + endpoint->input_signal.anchor.y &&
           signal.direction == endpoint->input_signal.direction &&
           signal.phase == endpoint->input_signal.phase &&
           signal.generation == endpoint->scheduled_input_generation;
}

static void update_eater_route_canvas_flags(
    StraightRoute *route,
    SignalState signal)
{
    if (signal.phase != P0) return;

    int u;
    int v;

    if (anchor_to_canvas(signal.anchor, &u, &v)) 
    {
        route->has_entered_canvas = 1;
        return;
    }

    if (route->has_entered_canvas &&
        straight_route_has_passed_canvas(
            signal.anchor,
            route->direction))
        route->has_left_canvas = 1;
}

static VerificationResult observe_eater1_route(
    const uint8_t board[HEIGHT + 2][WIDTH + 2],
    GliderTracker *tracker,
    StraightRoute *route,
    EaterEndpoint *endpoint,
    uint64_t generation)
{
    if (tracker->state == TRACKER_FAILED)
        return tracker->failure_reason;

    if (tracker->state == TRACKER_COMPLETED)
        return VERIFY_OK;

    if (tracker->state == TRACKER_ABSORBED) 
    {
        if (!board_equals_only_pattern(
                board,
                &eater1_pattern,
                endpoint->origin)) 
        {
            fail_tracker(
                tracker,
                generation,
                VERIFY_EATER_PATTERN_MISMATCH
            );
            return VERIFY_EATER_PATTERN_MISMATCH;
        }

        return VERIFY_OK;
    }

    if (tracker->last_processed_generation == generation)
        return VERIFY_OK;

    if (tracker->state == TRACKER_ENTERING_COMPONENT) 
    {
        uint64_t expected_generation =
            tracker->component_input_generation +
            endpoint->reaction_start_offset;

        if (generation != expected_generation) 
        {
            fail_tracker(
                tracker,
                generation,
                VERIFY_WRONG_GENERATION
            );
            return VERIFY_WRONG_GENERATION;
        }

        VerificationResult result = verify_eater1_reaction_frame(
            board,
            endpoint,
            0
        );

        if (result != VERIFY_OK) 
        {
            fail_tracker(tracker, generation, result);
            return result;
        }

        tracker->last_processed_generation = generation;

        if (endpoint->reaction_frames.frame_count == 1) 
        {
            if (!board_equals_only_pattern(
                    board,
                    &eater1_pattern,
                    endpoint->origin)) 
            {
                fail_tracker(
                    tracker,
                    generation,
                    VERIFY_EATER_PATTERN_MISMATCH
                );
                return VERIFY_EATER_PATTERN_MISMATCH;
            }

            tracker->state = TRACKER_ABSORBED;
            return VERIFY_OK;
        }

        tracker->state = TRACKER_IN_REACTION;
        tracker->reaction_tick = 1;
        return VERIFY_OK;
    }

    if (tracker->state == TRACKER_IN_REACTION) 
    {
        size_t frame_index = (size_t)tracker->reaction_tick;
        uint64_t expected_generation =
            tracker->component_input_generation +
            endpoint->reaction_start_offset + frame_index;

        if (generation != expected_generation) 
        {
            fail_tracker(
                tracker,
                generation,
                VERIFY_WRONG_GENERATION
            );
            return VERIFY_WRONG_GENERATION;
        }

        VerificationResult result = verify_eater1_reaction_frame(
            board,
            endpoint,
            frame_index
        );

        if (result != VERIFY_OK) 
        {
            fail_tracker(tracker, generation, result);
            return result;
        }

        tracker->last_processed_generation = generation;

        if (frame_index + 1 ==
            endpoint->reaction_frames.frame_count) 
        {
            if (!board_equals_only_pattern(
                    board,
                    &eater1_pattern,
                    endpoint->origin)) 
            {
                fail_tracker(
                    tracker,
                    generation,
                    VERIFY_EATER_PATTERN_MISMATCH
                );
                return VERIFY_EATER_PATTERN_MISMATCH;
            }

            tracker->state = TRACKER_ABSORBED;
            return VERIFY_OK;
        }

        tracker->reaction_tick += 1;
        return VERIFY_OK;
    }

    if (tracker->state != TRACKER_IN_FLIGHT ||
        tracker->expected.direction != route->direction) 
    {
        fail_tracker(
            tracker,
            generation,
            VERIFY_OUT_OF_EXPECTED_STATE
        );
        return VERIFY_OUT_OF_EXPECTED_STATE;
    }

    if (generation > endpoint->scheduled_input_generation) 
    {
        fail_tracker(
            tracker,
            generation,
            VERIFY_COMPONENT_CONTRACT_MISMATCH
        );
        return VERIFY_COMPONENT_CONTRACT_MISMATCH;
    }

    VerificationResult result = verify_glider(
        board,
        tracker,
        generation
    );

    if (result != VERIFY_OK) 
    {
        fail_tracker(tracker, generation, result);
        return result;
    }

    tracker->last_processed_generation = generation;
    update_eater_route_canvas_flags(route, tracker->expected);

    if (generation == endpoint->scheduled_input_generation) 
    {
        if (!signal_matches_eater1_input(
                tracker->expected,
                endpoint) ||
            !pattern_matches_at(
                board,
                &eater1_pattern,
                endpoint->origin)) 
        {
            fail_tracker(
                tracker,
                generation,
                VERIFY_COMPONENT_CONTRACT_MISMATCH
            );
            return VERIFY_COMPONENT_CONTRACT_MISMATCH;
        }

        tracker->drawing_enabled = 0;
        tracker->component_id = endpoint->pattern_id;
        tracker->component_input_generation = generation;
        tracker->reaction_tick = 0;
        tracker->state = TRACKER_ENTERING_COMPONENT;
        return VERIFY_OK;
    }

    maybe_emit_draw_event(tracker, generation, &event_log, trail);
    advance_verified_tracker(tracker);

    return VERIFY_OK;
}

static int build_eater1_collision_scenario(
    CollisionScenario *scenario)
{
    EaterEndpoint endpoint;
    StraightRoute route;

    if (!scenario || !configure_eater1_demo(&endpoint, &route))
        return 0;

    scenario->pattern = &eater1_pattern;
    scenario->pattern_origin = endpoint.origin;
    scenario->incoming_glider = route.initial_signal;
    scenario->simulation_limit =
        endpoint.scheduled_input_generation +
        endpoint.restore_generation_offset +
        EATER_STABILITY_GENERATIONS;

    return 1;
}

static int initialize_collision_run(
    CollisionRun *run,
    const CollisionScenario *scenario)
{
    if (!run) return 0;

    memset(run, 0, sizeof(*run));
    run->state = COLLISION_RUN_FAILED;

    if (!scenario ||
        scenario->pattern != &eater1_pattern ||
        scenario->incoming_glider.generation != 0 ||
        !configure_eater1_demo(
            &run->eater_endpoint,
            &run->route))
        return 0;

    if (scenario->pattern_origin.x != run->eater_endpoint.origin.x ||
        scenario->pattern_origin.y != run->eater_endpoint.origin.y ||
        !signal_states_equal(
            scenario->incoming_glider,
            run->route.initial_signal) ||
        glider_overlaps_pattern(
            scenario->incoming_glider,
            scenario->pattern,
            scenario->pattern_origin))
        return 0;

    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));

    place_pattern_generation0(
        board_a,
        scenario->pattern,
        scenario->pattern_origin
    );

    if (!place_glider_generation0(
            board_a,
            scenario->incoming_glider))
        return 0;

    run->scenario = *scenario;
    run->current = board_a;
    run->next = board_b;
    run->generation = 0;
    run->verification_result = VERIFY_OK;
    run->state = COLLISION_RUN_RUNNING;
    initialize_glider_tracker(
        &run->tracker,
        scenario->incoming_glider
    );
    run->tracker.drawing_enabled = 0;

    return 1;
}

static CollisionRunState observe_collision_run(CollisionRun *run)
{
    if (!run || run->state != COLLISION_RUN_RUNNING)
        return run ? run->state : COLLISION_RUN_FAILED;

    run->verification_result = observe_eater1_route(
        run->current,
        &run->tracker,
        &run->route,
        &run->eater_endpoint,
        run->generation
    );

    if (run->verification_result != VERIFY_OK ||
        run->tracker.state == TRACKER_FAILED) 
    {
        run->state = COLLISION_RUN_FAILED;
        return run->state;
    }

    if (run->tracker.state == TRACKER_ABSORBED) 
    {
        run->state = COLLISION_RUN_ABSORBED;
        return run->state;
    }

    if (run->generation >= run->scenario.simulation_limit)
        run->state = COLLISION_RUN_TIMEOUT;

    return run->state;
}

static int step_collision_run(CollisionRun *run)
{
    if (!run || run->state != COLLISION_RUN_RUNNING)
        return 0;

    if (run->generation >= run->scenario.simulation_limit) 
    {
        run->state = COLLISION_RUN_TIMEOUT;
        return 0;
    }

    step(run->current, run->next);

    uint8_t (*tmp)[WIDTH + 2] = run->current;
    run->current = run->next;
    run->next = tmp;
    run->generation += 1;

    return 1;
}

static CollisionRunState run_collision_headless(
    const CollisionScenario *scenario,
    CollisionRun *run)
{
    if (!initialize_collision_run(run, scenario))
        return COLLISION_RUN_FAILED;

    while (observe_collision_run(run) == COLLISION_RUN_RUNNING)
        step_collision_run(run);

    return run->state;
}

static const char *collision_run_state_name(CollisionRunState state)
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

static int phase_2a_pattern_matches(
    uint8_t board[HEIGHT + 2][WIDTH + 2],
    SignalState signal)
{
    for (int y = 0; y < GLIDER_PATTERN_SIZE; ++y) 
    {
        for (int x = 0; x < GLIDER_PATTERN_SIZE; ++x) 
        {
            int expected = 0;

            for (int i = 0; i < GLIDER_CELL_COUNT; ++i) 
            {
                CellOffset offset = glider_offsets
                    [signal.direction][signal.phase][i];

                if (offset.x == x && offset.y == y) 
                {
                    expected = 1;
                    break;
                }
            }

            int board_x = wrap_coord(signal.anchor.x + x, WIDTH);
            int board_y = wrap_coord(signal.anchor.y + y, HEIGHT);

            if ((board[board_y + 1][board_x + 1] != 0) != expected)
                return 0;
        }
    }

    return 1;
}

static int phase_2a_live_cell_count(
    uint8_t board[HEIGHT + 2][WIDTH + 2])
{
    int count = 0;

    for (int y = 1; y <= HEIGHT; ++y) 
    {
        for (int x = 1; x <= WIDTH; ++x)
            count += board[y][x] != 0;
    }

    return count;
}

static int phase_2a_test_canvas_mapping(void)
{
    for (int v = 0; v < CANVAS_HEIGHT; ++v) 
    {
        for (int u = 0; u < CANVAS_WIDTH; ++u) 
        {
            WorldAnchor anchor;
            int mapped_u;
            int mapped_v;

            if (!canvas_to_anchor(u, v, &anchor) ||
                !anchor_to_canvas(anchor, &mapped_u, &mapped_v) ||
                mapped_u != u || mapped_v != v)
                return 0;
        }
    }

    WorldAnchor anchor;
    int u;
    int v;

    canvas_to_anchor(0, 0, &anchor);
    anchor.x += 1;
    if (anchor_to_canvas(anchor, &u, &v)) return 0;

    canvas_to_anchor(0, 0, &anchor);
    anchor.y += 1;
    if (anchor_to_canvas(anchor, &u, &v)) return 0;

    anchor.x = CANVAS_X0 - CANVAS_PITCH;
    anchor.y = CANVAS_Y0;
    if (anchor_to_canvas(anchor, &u, &v)) return 0;

    canvas_to_anchor(0, 0, &anchor);
    anchor.x += WIDTH;
    if (anchor_to_canvas(anchor, &u, &v)) return 0;

    if (canvas_to_anchor(-1, 0, &anchor) ||
        canvas_to_anchor(0, -1, &anchor) ||
        canvas_to_anchor(CANVAS_WIDTH, 0, &anchor) ||
        canvas_to_anchor(0, CANVAS_HEIGHT, &anchor))
        return 0;

    return 1;
}

static int phase_2a_test_signal_advance(void)
{
    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) 
    {
        for (int phase = 0; phase < PHASE_COUNT; ++phase) 
        {
            SignalState signal =
            {
                {100, 100},
                (Direction)direction,
                (GliderPhase)phase,
                0
            };

            for (int generation = 1; generation <= 4; ++generation) 
            {
                signal = advance_signal(signal);

                if (signal.phase != (GliderPhase)((phase + generation) % PHASE_COUNT) ||
                    signal.generation != (uint64_t)generation)
                    return 0;
            }

            if (signal.anchor.x != 100 + direction_delta[direction].x ||
                signal.anchor.y != 100 + direction_delta[direction].y ||
                signal.phase != (GliderPhase)phase)
                return 0;
        }

        WorldAnchor anchor;
        int start_u = CANVAS_WIDTH / 2;
        int start_v = CANVAS_HEIGHT / 2;
        int mapped_u;
        int mapped_v;

        canvas_to_anchor(start_u, start_v, &anchor);

        SignalState signal =
        {
            anchor,
            (Direction)direction,
            P0,
            0
        };

        for (int generation = 0; generation < 4; ++generation)
            signal = advance_signal(signal);

        if (anchor_to_canvas(signal.anchor, &mapped_u, &mapped_v))
            return 0;

        for (int generation = 0; generation < 4; ++generation)
            signal = advance_signal(signal);

        if (!anchor_to_canvas(signal.anchor, &mapped_u, &mapped_v) ||
            mapped_u != start_u + direction_delta[direction].x ||
            mapped_v != start_v + direction_delta[direction].y ||
            signal.phase != P0 || signal.generation != 8)
            return 0;
    }

    return 1;
}

static int phase_2a_test_wrapped_placement(void)
{
    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    SignalState signal = {{-1, -1}, SE, P0, 0};

    if (!place_glider_generation0(current, signal)) return 0;

    for (int generation = 0; generation <= 4; ++generation) 
    {
        if (phase_2a_live_cell_count(current) != GLIDER_CELL_COUNT ||
            !phase_2a_pattern_matches(current, signal))
            return 0;

        if (generation == 4) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
        signal = advance_signal(signal);
    }

    return 1;
}

static int run_phase_2a_tests(void)
{
    if (!phase_2a_test_canvas_mapping()) 
    {
        fprintf(stderr, "Phase 2A canvas mapping test failed.\n");
        return 0;
    }

    if (!phase_2a_test_signal_advance()) 
    {
        fprintf(stderr, "Phase 2A signal advance test failed.\n");
        return 0;
    }

    if (!phase_2a_test_wrapped_placement()) 
    {
        fprintf(stderr, "Phase 2A torus placement test failed.\n");
        return 0;
    }

    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    SignalState signals[DIRECTION_COUNT * PHASE_COUNT];
    int signal_count = 0;

    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) 
    {
        for (int phase = 0; phase < PHASE_COUNT; ++phase) 
        {
            SignalState signal =
            {
                {32 + phase * 120, 24 + direction * 56},
                (Direction)direction,
                (GliderPhase)phase,
                0
            };

            if (!place_glider_generation0(current, signal)) 
            {
                fprintf(stderr, "Phase 2A generation-0 placement failed.\n");
                return 0;
            }

            signals[signal_count++] = signal;
        }
    }

    for (int generation = 0;
         generation <= PHASE_2A_TEST_GENERATIONS;
         ++generation) 
    {
        if (phase_2a_live_cell_count(current) !=
            DIRECTION_COUNT * PHASE_COUNT * GLIDER_CELL_COUNT) 
        {
            fprintf(
                stderr,
                "Phase 2A live-cell count failed at generation %d.\n",
                generation
            );
            return 0;
        }

        for (int i = 0; i < signal_count; ++i) 
        {
            if (signals[i].generation != (uint64_t)generation ||
                !phase_2a_pattern_matches(current, signals[i])) 
            {
                fprintf(
                    stderr,
                    "Phase 2A template failed at generation %d, direction %d, initial phase %d.\n",
                    generation,
                    i / PHASE_COUNT,
                    i % PHASE_COUNT
                );
                return 0;
            }
        }

        if (generation == PHASE_2A_TEST_GENERATIONS) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;

        for (int i = 0; i < signal_count; ++i)
            signals[i] = advance_signal(signals[i]);
    }

    printf(
        "Phase 2A passed: 16 initial glider states, %d generations, canvas mapping, and torus placement.\n",
        PHASE_2A_TEST_GENERATIONS
    );

    return 1;
}

static int signal_states_equal(SignalState a, SignalState b)
{
    return a.anchor.x == b.anchor.x &&
           a.anchor.y == b.anchor.y &&
           a.direction == b.direction &&
           a.phase == b.phase &&
           a.generation == b.generation;
}

static int trackers_equal(GliderTracker a, GliderTracker b)
{
    return signal_states_equal(a.expected, b.expected) &&
           a.state == b.state &&
           a.tracker_id == b.tracker_id &&
           a.drawing_enabled == b.drawing_enabled &&
           a.last_processed_generation == b.last_processed_generation &&
           a.last_draw_generation == b.last_draw_generation &&
           a.last_draw_u == b.last_draw_u &&
           a.last_draw_v == b.last_draw_v &&
           a.component_id == b.component_id &&
           a.component_input_generation ==
               b.component_input_generation &&
           a.reaction_tick == b.reaction_tick &&
           a.failure_generation == b.failure_generation &&
           a.failure_reason == b.failure_reason;
}

static void phase_2b_set_test_cell(
    uint8_t board[HEIGHT + 2][WIDTH + 2],
    int64_t world_x,
    int64_t world_y,
    uint8_t alive)
{
    int board_x = wrap_coord(world_x, WIDTH);
    int board_y = wrap_coord(world_y, HEIGHT);

    board[board_y + 1][board_x + 1] = alive;
}

static int phase_2b_run_verified_glider(
    SignalState initial,
    int generation_count)
{
    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    GliderTracker tracker;
    SignalState expected = initial;

    if (!place_glider_generation0(current, initial)) return 0;

    initialize_glider_tracker(&tracker, initial);

    for (int generation = 0;
         generation <= generation_count;
         ++generation) 
    {
        if (!signal_states_equal(tracker.expected, expected)) return 0;

        VerificationResult result = observe_tracker(
            current,
            &tracker,
            (uint64_t)generation
        );

        if (result != VERIFY_OK ||
            tracker.state != TRACKER_IN_FLIGHT ||
            tracker.last_processed_generation != (uint64_t)generation)
            return 0;

        expected = advance_signal(expected);

        if (!signal_states_equal(tracker.expected, expected)) return 0;

        if (generation == generation_count) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    return 1;
}

static int phase_2b_test_all_initial_states(void)
{
    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) 
    {
        for (int phase = 0; phase < PHASE_COUNT; ++phase) 
        {
            SignalState initial =
            {
                {120, 100},
                (Direction)direction,
                (GliderPhase)phase,
                0
            };

            if (!phase_2b_run_verified_glider(
                    initial,
                    PHASE_2A_TEST_GENERATIONS))
                return 0;
        }
    }

    return 1;
}

static int phase_2b_test_missing_expected_cell(void)
{
    memset(board_a, 0, sizeof(board_a));

    SignalState initial = {{100, 80}, SE, P0, 0};
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, initial)) return 0;

    CellOffset missing = glider_offsets[SE][P0][0];
    phase_2b_set_test_cell(
        board_a,
        initial.anchor.x + missing.x,
        initial.anchor.y + missing.y,
        0
    );

    initialize_glider_tracker(&tracker, initial);

    VerificationResult result = observe_tracker(board_a, &tracker, 0);

    if (result != VERIFY_MISSING_EXPECTED_LIVE ||
        tracker.state != TRACKER_FAILED ||
        tracker.failure_generation != 0 ||
        tracker.failure_reason != result ||
        !signal_states_equal(tracker.expected, initial))
        return 0;

    GliderTracker after_failure = tracker;

    phase_2b_set_test_cell(
        board_a,
        initial.anchor.x + missing.x,
        initial.anchor.y + missing.y,
        1
    );

    return observe_tracker(board_a, &tracker, 0) == result &&
           trackers_equal(tracker, after_failure);
}

static int phase_2b_test_extra_bounding_box_cell(void)
{
    memset(board_a, 0, sizeof(board_a));

    SignalState initial = {{100, 80}, SE, P0, 0};
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, initial)) return 0;

    phase_2b_set_test_cell(
        board_a,
        initial.anchor.x,
        initial.anchor.y,
        1
    );
    initialize_glider_tracker(&tracker, initial);

    VerificationResult result = observe_tracker(board_a, &tracker, 0);

    return result == VERIFY_EXTRA_BOUNDING_BOX_LIVE &&
           tracker.state == TRACKER_FAILED &&
           tracker.failure_reason == result &&
           signal_states_equal(tracker.expected, initial);
}

static int phase_2b_test_isolation_violation(void)
{
    memset(board_a, 0, sizeof(board_a));

    SignalState initial = {{100, 80}, SE, P0, 0};
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, initial)) return 0;

    phase_2b_set_test_cell(
        board_a,
        initial.anchor.x - 1,
        initial.anchor.y - 1,
        1
    );
    initialize_glider_tracker(&tracker, initial);

    VerificationResult result = observe_tracker(board_a, &tracker, 0);

    return result == VERIFY_ISOLATION_VIOLATION &&
           tracker.state == TRACKER_FAILED &&
           tracker.failure_reason == result &&
           signal_states_equal(tracker.expected, initial);
}

static int phase_2b_test_distant_cell(void)
{
    memset(board_a, 0, sizeof(board_a));

    SignalState initial = {{100, 80}, SE, P0, 0};
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, initial)) return 0;

    phase_2b_set_test_cell(
        board_a,
        initial.anchor.x + 10,
        initial.anchor.y + 10,
        1
    );
    initialize_glider_tracker(&tracker, initial);

    return verify_glider(board_a, &tracker, 0) == VERIFY_OK;
}

static int phase_2b_test_wrong_phase(void)
{
    memset(board_a, 0, sizeof(board_a));

    SignalState actual = {{100, 80}, SE, P0, 0};
    SignalState expected = actual;
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, actual)) return 0;

    expected.phase = P1;
    initialize_glider_tracker(&tracker, expected);

    VerificationResult result = observe_tracker(board_a, &tracker, 0);

    return result != VERIFY_OK &&
           tracker.state == TRACKER_FAILED &&
           signal_states_equal(tracker.expected, expected);
}

static int phase_2b_test_wrong_anchor(void)
{
    memset(board_a, 0, sizeof(board_a));

    SignalState actual = {{100, 80}, SE, P0, 0};
    SignalState expected = actual;
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, actual)) return 0;

    expected.anchor.x += 1;
    initialize_glider_tracker(&tracker, expected);

    VerificationResult result = observe_tracker(board_a, &tracker, 0);

    return result != VERIFY_OK &&
           tracker.state == TRACKER_FAILED &&
           signal_states_equal(tracker.expected, expected);
}

static int phase_2b_test_wrong_generation(void)
{
    memset(board_a, 0, sizeof(board_a));

    SignalState actual = {{100, 80}, SE, P0, 0};
    SignalState expected = actual;
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, actual)) return 0;

    expected.generation = 1;
    initialize_glider_tracker(&tracker, expected);

    VerificationResult result = observe_tracker(board_a, &tracker, 0);

    return result == VERIFY_WRONG_GENERATION &&
           tracker.state == TRACKER_FAILED &&
           tracker.failure_generation == 0 &&
           tracker.failure_reason == result &&
           signal_states_equal(tracker.expected, expected);
}

static int phase_2b_test_repeated_observation(void)
{
    memset(board_a, 0, sizeof(board_a));

    SignalState initial = {{100, 80}, SE, P0, 0};
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, initial)) return 0;

    initialize_glider_tracker(&tracker, initial);

    if (observe_tracker(board_a, &tracker, 0) != VERIFY_OK)
        return 0;

    GliderTracker after_first_observation = tracker;

    if (observe_tracker(board_a, &tracker, 0) != VERIFY_OK)
        return 0;

    return trackers_equal(tracker, after_first_observation);
}

static int phase_2b_test_torus_boundaries(void)
{
    static const WorldAnchor anchors[] =
    {
        {-1, 64},
        {WIDTH - 1, 64},
        {64, -1},
        {64, HEIGHT - 1},
        {-1, -1},
        {WIDTH - 1, -1},
        {-1, HEIGHT - 1},
        {WIDTH - 1, HEIGHT - 1},
        {WIDTH + 1, HEIGHT + 1}
    };

    int anchor_count = (int)(sizeof(anchors) / sizeof(anchors[0]));

    for (int i = 0; i < anchor_count; ++i) 
    {
        SignalState initial = {anchors[i], SE, P0, 0};

        if (!phase_2b_run_verified_glider(initial, 8)) return 0;
    }

    return 1;
}

static int run_phase_2b_tests(void)
{
    if (!run_phase_2a_tests()) 
    {
        fprintf(stderr, "Phase 2B prerequisite Phase 2A regression failed.\n");
        return 0;
    }

    SignalState initial = {{100, 80}, SE, P0, 0};

    if (!phase_2b_run_verified_glider(
            initial,
            PHASE_2B_TEST_GENERATIONS)) 
    {
        fprintf(stderr, "Phase 2B SE/P0 continuous verification failed.\n");
        return 0;
    }

    if (!phase_2b_test_all_initial_states()) 
    {
        fprintf(stderr, "Phase 2B direction/phase coverage failed.\n");
        return 0;
    }

    if (!phase_2b_test_missing_expected_cell()) 
    {
        fprintf(stderr, "Phase 2B missing-cell negative test failed.\n");
        return 0;
    }

    if (!phase_2b_test_extra_bounding_box_cell()) 
    {
        fprintf(stderr, "Phase 2B extra-cell negative test failed.\n");
        return 0;
    }

    if (!phase_2b_test_isolation_violation()) 
    {
        fprintf(stderr, "Phase 2B isolation negative test failed.\n");
        return 0;
    }

    if (!phase_2b_test_distant_cell()) 
    {
        fprintf(stderr, "Phase 2B distant-cell test failed.\n");
        return 0;
    }

    if (!phase_2b_test_wrong_phase()) 
    {
        fprintf(stderr, "Phase 2B wrong-phase negative test failed.\n");
        return 0;
    }

    if (!phase_2b_test_wrong_anchor()) 
    {
        fprintf(stderr, "Phase 2B wrong-anchor negative test failed.\n");
        return 0;
    }

    if (!phase_2b_test_wrong_generation()) 
    {
        fprintf(stderr, "Phase 2B wrong-generation negative test failed.\n");
        return 0;
    }

    if (!phase_2b_test_repeated_observation()) 
    {
        fprintf(stderr, "Phase 2B repeated-observation test failed.\n");
        return 0;
    }

    if (!phase_2b_test_torus_boundaries()) 
    {
        fprintf(stderr, "Phase 2B torus-boundary test failed.\n");
        return 0;
    }

    printf(
        "Phase 2B passed: continuous verification, 16 initial states, negative cases, generation deduplication, and torus boundaries.\n"
    );

    return 1;
}

static int reset_world(
    uint8_t (**current)[WIDTH + 2],
    uint8_t (**next)[WIDTH + 2],
    uint64_t *generation,
    int *paused,
    GliderTracker *tracker,
    StraightRoute *route)
{
    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));
    reset_drawing_state();

    if (!plan_straight_route(SE, 0, 0, route)) return 0;

    if (!place_glider_generation0(board_a, route->initial_signal))
        return 0;

    initialize_glider_tracker(tracker, route->initial_signal);

    *current = board_a;
    *next = board_b;
    *generation = 0;
    *paused = 0;

    return 1;
}

static int reset_eater1_world(
    uint8_t (**current)[WIDTH + 2],
    uint8_t (**next)[WIDTH + 2],
    uint64_t *generation,
    int *paused,
    GliderTracker *tracker,
    StraightRoute *route,
    EaterEndpoint *endpoint)
{
    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));
    reset_drawing_state();

    if (!configure_eater1_demo(endpoint, route)) return 0;

    place_pattern_generation0(
        board_a,
        &eater1_pattern,
        endpoint->origin
    );

    if (!place_glider_generation0(board_a, route->initial_signal))
        return 0;

    initialize_glider_tracker(tracker, route->initial_signal);

    *current = board_a;
    *next = board_b;
    *generation = 0;
    *paused = 0;

    return 1;
}

static int phase_3_prepare_single_world(
    SignalState initial,
    GliderTracker *tracker)
{
    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));
    reset_drawing_state();

    if (!place_glider_generation0(board_a, initial)) return 0;

    initialize_glider_tracker(tracker, initial);

    return 1;
}

static int phase_3_trail_is_clear(void)
{
    for (int v = 0; v < CANVAS_HEIGHT; ++v) 
    {
        for (int u = 0; u < CANVAS_WIDTH; ++u) 
        {
            if (trail[v][u]) return 0;
        }
    }

    return 1;
}

static int phase_3_boards_equal(
    const uint8_t first[HEIGHT + 2][WIDTH + 2],
    const uint8_t second[HEIGHT + 2][WIDTH + 2])
{
    for (int y = 1; y <= HEIGHT; ++y) 
    {
        for (int x = 1; x <= WIDTH; ++x) 
        {
            if (first[y][x] != second[y][x]) return 0;
        }
    }

    return 1;
}

static int phase_3_test_direction(Direction direction)
{
    int start_u =
        direction == SW || direction == NW ? 80 : 40;
    int start_v =
        direction == NE || direction == NW ? 32 : 8;
    WorldAnchor anchor;

    if (!canvas_to_anchor(start_u, start_v, &anchor)) return 0;

    SignalState initial = {anchor, direction, P0, 0};
    GliderTracker tracker;

    if (!phase_3_prepare_single_world(initial, &tracker)) return 0;

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;

    for (int generation = 0;
         generation <= PHASE_3_TEST_GENERATIONS;
         ++generation) 
    {
        SignalState observed = tracker.expected;
        int expected_u;
        int expected_v;
        int should_draw =
            observed.phase == P0 &&
            anchor_to_canvas(
                observed.anchor,
                &expected_u,
                &expected_v
            );
        size_t event_count_before = event_log.count;

        if (verify_glider(current, &tracker, (uint64_t)generation) !=
            VERIFY_OK)
            return 0;

        if (observe_tracker(current, &tracker, (uint64_t)generation) !=
            VERIFY_OK)
            return 0;

        if (event_log.count != event_count_before + (size_t)should_draw)
            return 0;

        if (should_draw) 
        {
            DrawEvent event = event_log.events[event_log.count - 1];

            if (event.generation != (uint64_t)generation ||
                event.tracker_id != tracker.tracker_id ||
                event.canvas_u != expected_u ||
                event.canvas_v != expected_v ||
                event.anchor.x != observed.anchor.x ||
                event.anchor.y != observed.anchor.y ||
                event.direction != direction ||
                event.phase != P0)
                return 0;
        }

        if (generation == PHASE_3_TEST_GENERATIONS) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    if (event_log.count < 2) return 0;

    for (size_t i = 1; i < event_log.count; ++i) 
    {
        DrawEvent previous = event_log.events[i - 1];
        DrawEvent current_event = event_log.events[i];

        if (current_event.generation - previous.generation != 8 ||
            current_event.canvas_u - previous.canvas_u !=
                direction_delta[direction].x ||
            current_event.canvas_v - previous.canvas_v !=
                direction_delta[direction].y)
            return 0;
    }

    return 1;
}

static int phase_3_test_four_directions(void)
{
    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) 
    {
        if (!phase_3_test_direction((Direction)direction)) return 0;
    }

    return 1;
}

static int phase_3_test_noncanonical_phases(void)
{
    WorldAnchor first_draw_anchor;

    if (!canvas_to_anchor(
            CANVAS_WIDTH / 2,
            CANVAS_HEIGHT / 2,
            &first_draw_anchor))
        return 0;

    for (int phase = P1; phase <= P3; ++phase) 
    {
        SignalState offset = {{0, 0}, SE, (GliderPhase)phase, 0};

        while (offset.phase != P0)
            offset = advance_signal(offset);

        SignalState initial =
        {
            {
                first_draw_anchor.x - offset.anchor.x,
                first_draw_anchor.y - offset.anchor.y
            },
            SE,
            (GliderPhase)phase,
            0
        };
        GliderTracker tracker;

        if (!phase_3_prepare_single_world(initial, &tracker)) return 0;

        uint8_t (*current)[WIDTH + 2] = board_a;
        uint8_t (*next)[WIDTH + 2] = board_b;

        for (int generation = 0; generation <= 16; ++generation) 
        {
            SignalState observed = tracker.expected;
            size_t event_count_before = event_log.count;

            if (verify_glider(current, &tracker, (uint64_t)generation) !=
                VERIFY_OK)
                return 0;

            if (observe_tracker(current, &tracker, (uint64_t)generation) !=
                VERIFY_OK)
                return 0;

            if (observed.phase != P0 &&
                event_log.count != event_count_before)
                return 0;

            if (event_log.count > event_count_before &&
                event_log.events[event_log.count - 1].phase != P0)
                return 0;

            if (generation == 16) break;

            step(current, next);

            uint8_t (*tmp)[WIDTH + 2] = current;
            current = next;
            next = tmp;
        }

        if (event_log.count == 0) return 0;

        for (size_t i = 0; i < event_log.count; ++i) 
        {
            if (event_log.events[i].phase != P0) return 0;
        }
    }

    return 1;
}

static int phase_3_test_repeated_observation(void)
{
    WorldAnchor anchor;

    if (!canvas_to_anchor(32, 16, &anchor)) return 0;

    SignalState initial = {anchor, SE, P0, 0};
    GliderTracker tracker;

    if (!phase_3_prepare_single_world(initial, &tracker)) return 0;

    if (observe_tracker(board_a, &tracker, 0) != VERIFY_OK ||
        event_log.count != 1 ||
        trail[16][32] != 1)
        return 0;

    GliderTracker after_first_observation = tracker;

    if (observe_tracker(board_a, &tracker, 0) != VERIFY_OK ||
        observe_tracker(board_a, &tracker, 0) != VERIFY_OK)
        return 0;

    return event_log.count == 1 &&
           trail[16][32] == 1 &&
           trackers_equal(tracker, after_first_observation);
}

static int phase_3_test_double_xor(void)
{
    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));
    reset_drawing_state();

    int target_u = CANVAS_WIDTH / 2;
    int target_v = CANVAS_HEIGHT / 2;
    WorldAnchor target_anchor;

    if (!canvas_to_anchor(target_u, target_v, &target_anchor)) return 0;

    SignalState first_initial = {target_anchor, SE, P0, 0};
    SignalState second_initial =
    {
        {target_anchor.x - 8, target_anchor.y - 8},
        SE,
        P0,
        0
    };
    GliderTracker first_tracker;
    GliderTracker second_tracker;

    if (!place_glider_generation0(board_a, first_initial) ||
        !place_glider_generation0(board_a, second_initial))
        return 0;

    initialize_glider_tracker(&first_tracker, first_initial);
    initialize_glider_tracker(&second_tracker, second_initial);
    first_tracker.tracker_id = 1;
    second_tracker.tracker_id = 2;

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;

    for (int generation = 0; generation <= 32; ++generation) 
    {
        if (verify_glider(current, &first_tracker, (uint64_t)generation) !=
                VERIFY_OK ||
            verify_glider(current, &second_tracker, (uint64_t)generation) !=
                VERIFY_OK)
            return 0;

        if (observe_tracker(
                current,
                &first_tracker,
                (uint64_t)generation) != VERIFY_OK ||
            observe_tracker(
                current,
                &second_tracker,
                (uint64_t)generation) != VERIFY_OK)
            return 0;

        if (generation == 0 && trail[target_v][target_u] != 1)
            return 0;

        if (generation == 32) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    int target_event_count = 0;
    int saw_first_tracker = 0;
    int saw_second_tracker = 0;

    for (size_t i = 0; i < event_log.count; ++i) 
    {
        DrawEvent event = event_log.events[i];

        if (event.canvas_u != target_u || event.canvas_v != target_v)
            continue;

        target_event_count += 1;
        saw_first_tracker |= event.tracker_id == 1;
        saw_second_tracker |= event.tracker_id == 2;
    }

    return target_event_count == 2 &&
           saw_first_tracker &&
           saw_second_tracker &&
           trail[target_v][target_u] == 0;
}

static int phase_3_test_failed_tracker(void)
{
    int u = 24;
    int v = 12;
    WorldAnchor anchor;

    if (!canvas_to_anchor(u, v, &anchor)) return 0;

    SignalState initial = {anchor, SE, P0, 0};
    GliderTracker tracker;

    if (!phase_3_prepare_single_world(initial, &tracker)) return 0;

    CellOffset missing = glider_offsets[SE][P0][0];
    phase_2b_set_test_cell(
        board_a,
        initial.anchor.x + missing.x,
        initial.anchor.y + missing.y,
        0
    );

    if (observe_tracker(board_a, &tracker, 0) !=
            VERIFY_MISSING_EXPECTED_LIVE ||
        tracker.state != TRACKER_FAILED ||
        event_log.count != 0 ||
        !phase_3_trail_is_clear())
        return 0;

    phase_2b_set_test_cell(
        board_a,
        initial.anchor.x + missing.x,
        initial.anchor.y + missing.y,
        1
    );

    return observe_tracker(board_a, &tracker, 0) ==
               VERIFY_MISSING_EXPECTED_LIVE &&
           event_log.count == 0 &&
           phase_3_trail_is_clear();
}

static int phase_3_test_drawing_disabled(void)
{
    WorldAnchor anchor;

    if (!canvas_to_anchor(24, 12, &anchor)) return 0;

    SignalState initial = {anchor, SE, P0, 0};
    GliderTracker tracker;

    if (!phase_3_prepare_single_world(initial, &tracker)) return 0;

    tracker.drawing_enabled = 0;

    return observe_tracker(board_a, &tracker, 0) == VERIFY_OK &&
           tracker.state == TRACKER_IN_FLIGHT &&
           tracker.expected.generation == 1 &&
           event_log.count == 0 &&
           phase_3_trail_is_clear();
}

static int phase_3_test_off_lattice_anchor(void)
{
    int start_u = 40;
    int start_v = 16;
    WorldAnchor lattice_anchor;

    if (!canvas_to_anchor(start_u, start_v, &lattice_anchor)) return 0;

    SignalState initial =
    {
        {lattice_anchor.x + 1, lattice_anchor.y + 1},
        SE,
        P0,
        0
    };
    GliderTracker tracker;

    if (!phase_3_prepare_single_world(initial, &tracker)) return 0;

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;

    for (int generation = 0; generation <= 4; ++generation) 
    {
        if (observe_tracker(current, &tracker, (uint64_t)generation) !=
            VERIFY_OK)
            return 0;

        if (generation == 0 && event_log.count != 0) return 0;

        if (generation == 4) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    return event_log.count == 1 &&
           event_log.events[0].generation == 4 &&
           event_log.events[0].phase == P0 &&
           event_log.events[0].canvas_u == start_u + 1 &&
           event_log.events[0].canvas_v == start_v + 1;
}

static int phase_3_test_event_log_rebuild(void)
{
    if (!phase_3_test_direction(SE)) return 0;

    uint8_t rebuilt[CANVAS_HEIGHT][CANVAS_WIDTH];

    if (!rebuild_trail_from_event_log(rebuilt, &event_log)) return 0;

    return memcmp(rebuilt, trail, sizeof(trail)) == 0;
}

static int phase_3_test_trail_board_independence(void)
{
    static uint8_t control_a[HEIGHT + 2][WIDTH + 2];
    static uint8_t control_b[HEIGHT + 2][WIDTH + 2];

    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));
    memset(control_a, 0, sizeof(control_a));
    memset(control_b, 0, sizeof(control_b));
    reset_drawing_state();

    WorldAnchor anchor;

    if (!canvas_to_anchor(40, 12, &anchor)) return 0;

    SignalState initial = {anchor, SE, P0, 0};
    GliderTracker tracker;

    if (!place_glider_generation0(board_a, initial) ||
        !place_glider_generation0(control_a, initial))
        return 0;

    initialize_glider_tracker(&tracker, initial);

    uint8_t (*drawing_current)[WIDTH + 2] = board_a;
    uint8_t (*drawing_next)[WIDTH + 2] = board_b;
    uint8_t (*control_current)[WIDTH + 2] = control_a;
    uint8_t (*control_next)[WIDTH + 2] = control_b;

    for (int generation = 0;
         generation <= PHASE_3_TEST_GENERATIONS;
         ++generation) 
    {
        if (!phase_3_boards_equal(drawing_current, control_current))
            return 0;

        if (observe_tracker(
                drawing_current,
                &tracker,
                (uint64_t)generation) != VERIFY_OK)
            return 0;

        if (!phase_3_boards_equal(drawing_current, control_current))
            return 0;

        if (generation == PHASE_3_TEST_GENERATIONS) break;

        step(drawing_current, drawing_next);
        step(control_current, control_next);

        uint8_t (*drawing_tmp)[WIDTH + 2] = drawing_current;
        drawing_current = drawing_next;
        drawing_next = drawing_tmp;

        uint8_t (*control_tmp)[WIDTH + 2] = control_current;
        control_current = control_next;
        control_next = control_tmp;
    }

    return event_log.count > 0 &&
           !phase_3_trail_is_clear() &&
           phase_3_boards_equal(drawing_current, control_current);
}

static int phase_3_test_reset(void)
{
    WorldAnchor anchor;

    if (!canvas_to_anchor(20, 10, &anchor)) return 0;

    SignalState initial = {anchor, SE, P0, 0};
    GliderTracker tracker;

    if (!phase_3_prepare_single_world(initial, &tracker) ||
        observe_tracker(board_a, &tracker, 0) != VERIFY_OK ||
        event_log.count != 1)
        return 0;

    target[0][0] = 1;

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    uint64_t generation = 10;
    int paused = 1;
    StraightRoute route;

    if (!reset_world(
            &current,
            &next,
            &generation,
            &paused,
            &tracker,
            &route))
        return 0;

    int initial_u;
    int initial_v;

    return generation == 0 &&
           paused == 0 &&
           current == board_a &&
           next == board_b &&
           tracker.state == TRACKER_IN_FLIGHT &&
           tracker.expected.generation == 0 &&
           tracker.expected.phase == P0 &&
           signal_states_equal(
               tracker.expected,
               route.initial_signal
           ) &&
           !anchor_to_canvas(
               tracker.expected.anchor,
               &initial_u,
               &initial_v
           ) &&
           tracker.last_draw_generation ==
               TRACKER_GENERATION_UNPROCESSED &&
           !route.has_entered_canvas &&
           !route.has_left_canvas &&
           route.predicted_event_count > 0 &&
           event_log.count == 0 &&
           phase_3_trail_is_clear() &&
           target[0][0] == 1;
}

static int run_phase_3_tests(void)
{
    if (!run_phase_2b_tests()) 
    {
        fprintf(stderr, "Phase 3 prerequisite Phase 2 regression failed.\n");
        return 0;
    }

    if (!phase_3_test_direction(SE)) 
    {
        fprintf(stderr, "Phase 3 single-glider event interval test failed.\n");
        return 0;
    }

    if (!phase_3_test_four_directions()) 
    {
        fprintf(stderr, "Phase 3 direction delta test failed.\n");
        return 0;
    }

    if (!phase_3_test_noncanonical_phases()) 
    {
        fprintf(stderr, "Phase 3 canonical-phase test failed.\n");
        return 0;
    }

    if (!phase_3_test_repeated_observation()) 
    {
        fprintf(stderr, "Phase 3 repeated-observation test failed.\n");
        return 0;
    }

    if (!phase_3_test_double_xor()) 
    {
        fprintf(stderr, "Phase 3 double-XOR test failed.\n");
        return 0;
    }

    if (!phase_3_test_failed_tracker()) 
    {
        fprintf(stderr, "Phase 3 failed-tracker test failed.\n");
        return 0;
    }

    if (!phase_3_test_drawing_disabled()) 
    {
        fprintf(stderr, "Phase 3 drawing-permission test failed.\n");
        return 0;
    }

    if (!phase_3_test_off_lattice_anchor()) 
    {
        fprintf(stderr, "Phase 3 off-lattice anchor test failed.\n");
        return 0;
    }

    if (!phase_3_test_event_log_rebuild()) 
    {
        fprintf(stderr, "Phase 3 event-log rebuild test failed.\n");
        return 0;
    }

    if (!phase_3_test_trail_board_independence()) 
    {
        fprintf(stderr, "Phase 3 trail independence test failed.\n");
        return 0;
    }

    if (!phase_3_test_reset()) 
    {
        fprintf(stderr, "Phase 3 reset test failed.\n");
        return 0;
    }

    printf(
        "Phase 3 passed: verified P0 XOR events, four directions, deduplication, rebuild, and board independence.\n"
    );

    return 1;
}

static int phase_4_events_match_prediction(const StraightRoute *route)
{
    if (event_log.count != route->predicted_event_count) return 0;

    for (size_t i = 0; i < event_log.count; ++i) 
    {
        DrawEvent predicted = route->predicted_events[i];
        DrawEvent actual = event_log.events[i];

        if (actual.generation != predicted.generation ||
            actual.tracker_id != predicted.tracker_id ||
            actual.canvas_u != predicted.canvas_u ||
            actual.canvas_v != predicted.canvas_v ||
            actual.anchor.x != predicted.anchor.x ||
            actual.anchor.y != predicted.anchor.y ||
            actual.direction != predicted.direction ||
            actual.phase != predicted.phase)
            return 0;
    }

    return 1;
}

static int phase_4_mask_matches_prediction(const StraightRoute *route)
{
    return memcmp(
        route->predicted_mask.cells,
        trail,
        sizeof(trail)
    ) == 0;
}

static int phase_4_prediction_is_ordered(const StraightRoute *route)
{
    if (route->predicted_event_count == 0 ||
        route->expected_first_event_generation !=
            route->predicted_events[0].generation ||
        route->expected_last_event_generation !=
            route->predicted_events
                [route->predicted_event_count - 1].generation)
        return 0;

    for (size_t i = 1; i < route->predicted_event_count; ++i) 
    {
        DrawEvent previous = route->predicted_events[i - 1];
        DrawEvent current = route->predicted_events[i];

        if (current.generation - previous.generation != 8 ||
            current.canvas_u - previous.canvas_u !=
                direction_delta[route->direction].x ||
            current.canvas_v - previous.canvas_v !=
                direction_delta[route->direction].y)
            return 0;
    }

    return 1;
}

static int phase_4_has_no_earlier_entry(const StraightRoute *route)
{
    SignalState signal = route->initial_signal;

    for (uint64_t generation = 0;
         generation < route->expected_first_event_generation;
         ++generation) 
    {
        int u;
        int v;

        if (signal.phase == P0 &&
            anchor_to_canvas(signal.anchor, &u, &v))
            return 0;

        signal = advance_signal(signal);
    }

    int first_u;
    int first_v;

    return signal.generation ==
               route->expected_first_event_generation &&
           signal.phase == P0 &&
           anchor_to_canvas(signal.anchor, &first_u, &first_v) &&
           first_u == route->entry_u &&
           first_v == route->entry_v;
}

static int phase_4_run_planned_route(
    Direction direction,
    int entry_u,
    int entry_v,
    StraightRoute *completed_route)
{
    StraightRoute route;

    if (!plan_straight_route(direction, entry_u, entry_v, &route))
        return 0;

    int initial_u;
    int initial_v;

    if (anchor_to_canvas(
            route.initial_signal.anchor,
            &initial_u,
            &initial_v) ||
        route.initial_wrapped_anchor.x != wrap_coord(
            route.initial_signal.anchor.x,
            WIDTH) ||
        route.initial_wrapped_anchor.y != wrap_coord(
            route.initial_signal.anchor.y,
            HEIGHT) ||
        !phase_4_has_no_earlier_entry(&route) ||
        !phase_4_prediction_is_ordered(&route))
        return 0;

    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));
    reset_drawing_state();

    if (!place_glider_generation0(board_a, route.initial_signal))
        return 0;

    GliderTracker tracker;
    initialize_glider_tracker(&tracker, route.initial_signal);

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    uint64_t generation = 0;

    while (generation <= route.flight_generation_count) 
    {
        if (observe_straight_route(
                current,
                &tracker,
                &route,
                generation) != VERIFY_OK)
            return 0;

        if (tracker.state == TRACKER_COMPLETED) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
        generation += 1;
    }

    if (tracker.state != TRACKER_COMPLETED ||
        tracker.drawing_enabled ||
        !route.has_entered_canvas ||
        !route.has_left_canvas ||
        generation != route.flight_generation_count ||
        tracker.expected.generation != route.flight_generation_count ||
        tracker.expected.anchor.x != route.exit_anchor.x ||
        tracker.expected.anchor.y != route.exit_anchor.y ||
        tracker.last_draw_generation !=
            route.expected_last_event_generation ||
        !phase_4_events_match_prediction(&route) ||
        !phase_4_mask_matches_prediction(&route))
        return 0;

    if (completed_route) *completed_route = route;

    return 1;
}

static int phase_4_test_se_traversal(void)
{
    return phase_4_run_planned_route(SE, 0, 0, NULL);
}

static int phase_4_test_four_directions(void)
{
    static const struct
    {
        Direction direction;
        int entry_u;
        int entry_v;
    } cases[] =
    {
        {SE, 0, 12},
        {SW, CANVAS_WIDTH - 1, 12},
        {NE, 0, CANVAS_HEIGHT - 13},
        {NW, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 13}
    };

    int case_count = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < case_count; ++i) 
    {
        if (!phase_4_run_planned_route(
                cases[i].direction,
                cases[i].entry_u,
                cases[i].entry_v,
                NULL))
            return 0;
    }

    return 1;
}

static int phase_4_test_different_lanes(void)
{
    static const int entry_v[] = {0, 20, CANVAS_HEIGHT - 4};
    size_t previous_event_count = 0;

    for (int i = 0;
         i < (int)(sizeof(entry_v) / sizeof(entry_v[0]));
         ++i) 
    {
        StraightRoute route;

        if (!phase_4_run_planned_route(
                SE,
                0,
                entry_v[i],
                &route))
            return 0;

        if (route.entry_u != 0 ||
            route.entry_v != entry_v[i] ||
            route.predicted_event_count == 0)
            return 0;

        if (i > 0 &&
            route.predicted_event_count >= previous_event_count)
            return 0;

        previous_event_count = route.predicted_event_count;
    }

    return 1;
}

static int phase_4_test_completed_torus_return(void)
{
    StraightRoute route;

    if (!plan_straight_route(SE, 0, 12, &route)) return 0;

    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));
    reset_drawing_state();

    if (!place_glider_generation0(board_a, route.initial_signal))
        return 0;

    GliderTracker tracker;
    initialize_glider_tracker(&tracker, route.initial_signal);

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    uint64_t generation = 0;

    while (tracker.state == TRACKER_IN_FLIGHT &&
           generation <= route.flight_generation_count) 
    {
        if (observe_straight_route(
                current,
                &tracker,
                &route,
                generation) != VERIFY_OK)
            return 0;

        if (tracker.state == TRACKER_COMPLETED) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
        generation += 1;
    }

    if (tracker.state != TRACKER_COMPLETED ||
        generation != route.flight_generation_count ||
        !phase_4_events_match_prediction(&route) ||
        !phase_4_mask_matches_prediction(&route))
        return 0;

    SignalState physical_signal = tracker.expected;
    size_t completed_event_count = event_log.count;
    uint8_t completed_trail[CANVAS_HEIGHT][CANVAS_WIDTH];
    memcpy(completed_trail, trail, sizeof(completed_trail));
    int saw_wrapped_canvas_reentry = 0;

    while (generation < PHASE_4_WRAP_TEST_GENERATIONS) 
    {
        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
        generation += 1;
        physical_signal = advance_signal(physical_signal);

        if (!phase_2a_pattern_matches(current, physical_signal))
            return 0;

        if (observe_straight_route(
                current,
                &tracker,
                &route,
                generation) != VERIFY_OK)
            return 0;

        WorldAnchor wrapped_anchor =
        {
            wrap_coord(physical_signal.anchor.x, WIDTH),
            wrap_coord(physical_signal.anchor.y, HEIGHT)
        };
        int wrapped_u;
        int wrapped_v;
        int lifted_u;
        int lifted_v;

        if (physical_signal.phase == P0 &&
            anchor_to_canvas(
                wrapped_anchor,
                &wrapped_u,
                &wrapped_v) &&
            !anchor_to_canvas(
                physical_signal.anchor,
                &lifted_u,
                &lifted_v))
            saw_wrapped_canvas_reentry = 1;
    }

    return saw_wrapped_canvas_reentry &&
           tracker.state == TRACKER_COMPLETED &&
           !tracker.drawing_enabled &&
           event_log.count == completed_event_count &&
           memcmp(completed_trail, trail, sizeof(trail)) == 0 &&
           phase_2a_live_cell_count(current) == GLIDER_CELL_COUNT &&
           phase_2a_pattern_matches(current, physical_signal);
}

static int run_phase_4_tests(void)
{
    if (!run_phase_3_tests()) 
    {
        fprintf(stderr, "Phase 4 prerequisite Phase 1-3 regression failed.\n");
        return 0;
    }

    if (!phase_4_test_se_traversal()) 
    {
        fprintf(stderr, "Phase 4 SE traversal test failed.\n");
        return 0;
    }

    if (!phase_4_test_four_directions()) 
    {
        fprintf(stderr, "Phase 4 four-direction traversal test failed.\n");
        return 0;
    }

    if (!phase_4_test_different_lanes()) 
    {
        fprintf(stderr, "Phase 4 lane coverage test failed.\n");
        return 0;
    }

    if (!phase_4_test_completed_torus_return()) 
    {
        fprintf(stderr, "Phase 4 completed/torus-return test failed.\n");
        return 0;
    }

    printf(
        "Phase 4 passed: planned first traversals, ordered prediction, route masks, completion, and torus suppression.\n"
    );

    return 1;
}

static int phase_5a_test_canonical_replay(void)
{
    EaterEndpoint *endpoint = &canonical_eater1_endpoint;
    SignalState initial = endpoint->canonical_initial_signal;
    initial.anchor.x += endpoint->origin.x;
    initial.anchor.y += endpoint->origin.y;

    memset(component_board_a, 0, sizeof(component_board_a));
    memset(component_board_b, 0, sizeof(component_board_b));
    place_pattern_generation0(
        component_board_a,
        &eater1_pattern,
        endpoint->origin
    );

    if (!place_glider_generation0(component_board_a, initial))
        return 0;

    uint8_t (*current)[WIDTH + 2] = component_board_a;
    uint8_t (*next)[WIDTH + 2] = component_board_b;

    for (uint64_t generation = 0;
         generation <= endpoint->restore_generation;
         ++generation) 
    {
        if (generation >= endpoint->reaction_start_generation) 
        {
            size_t frame_index = (size_t)(
                generation - endpoint->reaction_start_generation
            );

            if (verify_eater1_reaction_frame(
                    current,
                    endpoint,
                    frame_index) != VERIFY_OK)
                return 0;
        }

        if (generation == endpoint->restore_generation) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    return board_equals_only_pattern(
        current,
        &eater1_pattern,
        endpoint->origin
    );
}

static int phase_5a_test_runtime_state_machine(void)
{
    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    uint64_t generation = 0;
    int paused = 0;
    GliderTracker tracker;
    StraightRoute route;

    if (!reset_eater1_world(
            &current,
            &next,
            &generation,
            &paused,
            &tracker,
            &route,
            &runtime_eater1_endpoint))
        return 0;

    int saw_entering = 0;
    int saw_reaction = 0;
    size_t entry_event_count = 0;
    uint8_t entry_trail[CANVAS_HEIGHT][CANVAS_WIDTH];
    uint64_t terminal_generation =
        runtime_eater1_endpoint.scheduled_input_generation +
        runtime_eater1_endpoint.restore_generation_offset + 1;

    while (generation <= terminal_generation) 
    {
        if (observe_eater1_route(
                current,
                &tracker,
                &route,
                &runtime_eater1_endpoint,
                generation) != VERIFY_OK)
            return 0;

        if (tracker.state == TRACKER_ENTERING_COMPONENT) 
        {
            if (!saw_entering) 
            {
                saw_entering = 1;
                entry_event_count = event_log.count;
                memcpy(entry_trail, trail, sizeof(entry_trail));
            }
        }
        else if (tracker.state == TRACKER_IN_REACTION) 
        {
            if (!saw_entering) return 0;
            saw_reaction = 1;
        }
        else if (tracker.state == TRACKER_ABSORBED) 
        {
            if (!saw_entering || !saw_reaction) return 0;
            break;
        }

        if (saw_entering &&
            (event_log.count != entry_event_count ||
             memcmp(entry_trail, trail, sizeof(trail)) != 0))
            return 0;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
        generation += 1;
    }

    if (tracker.state != TRACKER_ABSORBED ||
        tracker.drawing_enabled ||
        event_log.count != entry_event_count ||
        memcmp(entry_trail, trail, sizeof(trail)) != 0 ||
        !route.has_entered_canvas ||
        !route.has_left_canvas ||
        !phase_4_events_match_prediction(&route) ||
        !phase_4_mask_matches_prediction(&route) ||
        !board_equals_only_pattern(
            current,
            &eater1_pattern,
            runtime_eater1_endpoint.origin))
        return 0;

    for (int stable_generation = 0;
         stable_generation < 32;
         ++stable_generation) 
    {
        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
        generation += 1;

        if (observe_eater1_route(
                current,
                &tracker,
                &route,
                &runtime_eater1_endpoint,
                generation) != VERIFY_OK ||
            tracker.state != TRACKER_ABSORBED ||
            event_log.count != entry_event_count ||
            memcmp(entry_trail, trail, sizeof(trail)) != 0 ||
            !board_equals_only_pattern(
                current,
                &eater1_pattern,
                runtime_eater1_endpoint.origin))
            return 0;
    }

    return 1;
}

static int phase_5a_run_bad_input_case(
    int wrong_phase,
    int wrong_lane,
    int broken_eater)
{
    StraightRoute route;
    EaterEndpoint endpoint;

    if (!configure_eater1_demo(&endpoint, &route)) return 0;

    memset(board_a, 0, sizeof(board_a));
    memset(board_b, 0, sizeof(board_b));
    reset_drawing_state();
    place_pattern_generation0(
        board_a,
        &eater1_pattern,
        endpoint.origin
    );

    SignalState initial = route.initial_signal;

    if (wrong_phase)
        initial.phase = (GliderPhase)((initial.phase + 1) % PHASE_COUNT);

    if (wrong_lane) initial.anchor.x += 1;

    if (!place_glider_generation0(board_a, initial)) return 0;

    if (broken_eater) 
    {
        phase_2b_set_test_cell(
            board_a,
            endpoint.origin.x + eater1_cells[0].x,
            endpoint.origin.y + eater1_cells[0].y,
            0
        );
    }

    GliderTracker tracker;
    initialize_glider_tracker(&tracker, initial);

    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    uint64_t final_generation =
        endpoint.scheduled_input_generation +
        endpoint.restore_generation_offset + 16;

    for (uint64_t generation = 0;
         generation <= final_generation;
         ++generation) 
    {
        observe_eater1_route(
            current,
            &tracker,
            &route,
            &endpoint,
            generation
        );

        if (tracker.state == TRACKER_FAILED) return 1;
        if (tracker.state == TRACKER_ABSORBED) return 0;

        if (generation == final_generation) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
    }

    return tracker.state == TRACKER_FAILED;
}

static int phase_5a_test_extra_debris(void)
{
    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;
    uint64_t generation = 0;
    int paused = 0;
    GliderTracker tracker;
    StraightRoute route;

    if (!reset_eater1_world(
            &current,
            &next,
            &generation,
            &paused,
            &tracker,
            &route,
            &runtime_eater1_endpoint))
        return 0;

    while (tracker.state != TRACKER_ENTERING_COMPONENT) 
    {
        if (observe_eater1_route(
                current,
                &tracker,
                &route,
                &runtime_eater1_endpoint,
                generation) != VERIFY_OK ||
            tracker.state == TRACKER_FAILED)
            return 0;

        if (tracker.state == TRACKER_ENTERING_COMPONENT) break;

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;
        generation += 1;
    }

    size_t event_count_before = event_log.count;
    uint8_t trail_before[CANVAS_HEIGHT][CANVAS_WIDTH];
    memcpy(trail_before, trail, sizeof(trail_before));

    step(current, next);

    uint8_t (*tmp)[WIDTH + 2] = current;
    current = next;
    next = tmp;
    generation += 1;

    phase_2b_set_test_cell(
        current,
        runtime_eater1_endpoint.origin.x +
            runtime_eater1_endpoint.safety_bbox.min_x,
        runtime_eater1_endpoint.origin.y +
            runtime_eater1_endpoint.safety_bbox.min_y,
        1
    );

    VerificationResult result = observe_eater1_route(
        current,
        &tracker,
        &route,
        &runtime_eater1_endpoint,
        generation
    );

    return result == VERIFY_COMPONENT_SAFETY_VIOLATION &&
           tracker.state == TRACKER_FAILED &&
           event_log.count == event_count_before &&
           memcmp(trail_before, trail, sizeof(trail)) == 0;
}

static int run_phase_5a_tests(void)
{
    if (!run_phase_4_tests()) 
    {
        fprintf(stderr, "Phase 5A prerequisite Phase 1-4 regression failed.\n");
        return 0;
    }

    if (!verify_eater1_still_life()) 
    {
        fprintf(stderr, "Phase 5A Eater 1 still-life test failed.\n");
        return 0;
    }

    if (!ensure_eater1_contract()) 
    {
        fprintf(stderr, "Phase 5A Eater 1 port discovery failed.\n");
        return 0;
    }

    if (!phase_5a_test_canonical_replay()) 
    {
        fprintf(stderr, "Phase 5A canonical reaction replay failed.\n");
        return 0;
    }

    if (!phase_5a_test_runtime_state_machine()) 
    {
        fprintf(stderr, "Phase 5A runtime state-machine test failed.\n");
        return 0;
    }

    if (!phase_5a_run_bad_input_case(1, 0, 0)) 
    {
        fprintf(stderr, "Phase 5A wrong-phase test failed.\n");
        return 0;
    }

    if (!phase_5a_run_bad_input_case(0, 1, 0)) 
    {
        fprintf(stderr, "Phase 5A wrong-lane test failed.\n");
        return 0;
    }

    if (!phase_5a_run_bad_input_case(0, 0, 1)) 
    {
        fprintf(stderr, "Phase 5A broken-eater test failed.\n");
        return 0;
    }

    if (!phase_5a_test_extra_debris()) 
    {
        fprintf(stderr, "Phase 5A extra-debris test failed.\n");
        return 0;
    }

    printf(
        "Phase 5A passed: Eater 1 discovery, cached reaction, absorption state machine, and negative cases.\n"
    );

    return 1;
}

static void update_window_title(
    SDL_Window *window,
    const CollisionRun *run,
    int paused)
{
    char title[WINDOW_TITLE_SIZE];
    const char *state = collision_run_state_name(run->state);

    if (run->state == COLLISION_RUN_RUNNING && paused)
        state = "PAUSED";

    snprintf(
        title,
        sizeof(title),
        "Conway Collision Lab | Eater 1 | GEN: %llu / %llu | %s | Space Pause | N Step | R Reset | Esc Quit",
        (unsigned long long)run->generation,
        (unsigned long long)run->scenario.simulation_limit,
        state
    );

    SDL_SetWindowTitle(window, title);
}

static void render(
    SDL_Renderer *renderer,
    uint8_t board[HEIGHT + 2][WIDTH + 2])
{
    SDL_SetRenderDrawColor(renderer, 5, 8, 15, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 30, 45, 70, 255);

    for (int y = 0; y < HEIGHT; ++y) 
    {
        for (int x = 0; x < WIDTH; ++x) 
        {
            SDL_RenderDrawPoint(
                renderer,
                x * CELL_SIZE + CELL_SIZE / 2,
                y * CELL_SIZE + CELL_SIZE / 2
            );
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 80, 120, 255);

    for (int y = 1; y <= HEIGHT; ++y) 
    {
        for (int x = 1; x <= WIDTH; ++x) 
        {
            if (!board[y][x]) continue;

            SDL_Rect cell =
            {
                (x - 1) * CELL_SIZE,
                (y - 1) * CELL_SIZE,
                CELL_SIZE,
                CELL_SIZE
            };

            SDL_RenderFillRect(renderer, &cell);
        }
    }

    SDL_RenderPresent(renderer);
}

int main(void)
{
    CollisionScenario scenario;
    CollisionRun run;
    int paused = 0;

    if (PHASE_2A_TEST_MODE)
        return run_phase_2a_tests() ? 0 : 1;

    if (PHASE_2B_TEST_MODE)
        return run_phase_2b_tests() ? 0 : 1;

    if (PHASE_3_TEST_MODE)
        return run_phase_3_tests() ? 0 : 1;

    if (PHASE_4_TEST_MODE)
        return run_phase_4_tests() ? 0 : 1;

    if (PHASE_5A_TEST_MODE)
        return run_phase_5a_tests() ? 0 : 1;

    if (!build_eater1_collision_scenario(&scenario)) 
    {
        fprintf(stderr, "Unable to build the Eater 1 collision scenario.\n");
        return 1;
    }

    if (!initialize_collision_run(&run, &scenario)) 
    {
        fprintf(stderr, "Unable to initialize the collision run.\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) 
    {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Conway Collision Lab",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH * CELL_SIZE,
        HEIGHT * CELL_SIZE,
        0
    );

    if (!window) 
    {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED
    );

    if (!renderer) 
    {
        fprintf(stderr, "SDL renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int running = 1;
    SDL_Event event;

    while (running) 
    {
        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT) running = 0;

            if (event.type == SDL_KEYDOWN && !event.key.repeat) 
            {
                if (event.key.keysym.sym == SDLK_ESCAPE) running = 0;

                if (event.key.keysym.sym == SDLK_SPACE &&
                    run.state == COLLISION_RUN_RUNNING) 
                {
                    paused = !paused;
                }

                if (event.key.keysym.sym == SDLK_n &&
                    paused &&
                    run.state == COLLISION_RUN_RUNNING) 
                {
                    step_collision_run(&run);
                }

                if (event.key.keysym.sym == SDLK_r) 
                {
                    if (!initialize_collision_run(&run, &scenario)) 
                    {
                        fprintf(stderr, "Unable to reset the collision run.\n");
                        running = 0;
                    }
                    paused = 0;
                }
            }
        }

        if (!running) break;

        observe_collision_run(&run);
        update_window_title(window, &run, paused);
        render(renderer, run.current);

        if (!paused && run.state == COLLISION_RUN_RUNNING)
            step_collision_run(&run);

        SDL_Delay(DELAY_MS);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
