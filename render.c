#include "render.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DEMO_CELL_SIZE 2
#define DEMO_BOARD_PIXEL_WIDTH (LIFE_WIDTH * DEMO_CELL_SIZE)
#define DEMO_BOARD_PIXEL_HEIGHT (LIFE_HEIGHT * DEMO_CELL_SIZE)
#define BROWSER_LOGICAL_WIDTH 1080
#define BROWSER_LOGICAL_HEIGHT 760
#define BROWSER_MAX_CELL_SIZE 14
#define BROWSER_VIEW_PADDING_CELLS 4
#define BROWSER_VIEW_X 28
#define BROWSER_VIEW_Y 96
#define BROWSER_VIEW_WIDTH 700
#define BROWSER_VIEW_HEIGHT 532
#define BROWSER_INSPECTOR_X 756
#define BROWSER_INSPECTOR_Y 88
#define BROWSER_INSPECTOR_WIDTH 304
#define BROWSER_INSPECTOR_HEIGHT 548
#define BROWSER_FOOTER_Y 652
#define WINDOW_TITLE_SIZE 512

typedef struct
{
    char character;
    uint8_t rows[7];
} BitmapGlyph;

typedef struct
{
    WorldAnchor origin;
    int x;
    int y;
    int width;
    int height;
    int cell_size;
    int columns;
    int rows;
} BrowserViewport;

static int collision_renderer_initialize_size(
    CollisionRenderer *renderer,
    int width,
    int height,
    int resizable)
{
    if (!renderer) return 0;

    renderer->window = NULL;
    renderer->renderer = NULL;
    renderer->logical_width = width;
    renderer->logical_height = height;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 0;
    }

    renderer->window = SDL_CreateWindow(
        "LifeLab",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        resizable ? SDL_WINDOW_RESIZABLE : 0
    );

    if (!renderer->window)
    {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    renderer->renderer = SDL_CreateRenderer(
        renderer->window,
        -1,
        SDL_RENDERER_ACCELERATED
    );

    if (!renderer->renderer)
    {
        fprintf(stderr, "SDL renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(renderer->window);
        renderer->window = NULL;
        SDL_Quit();
        return 0;
    }

    SDL_SetRenderDrawBlendMode(
        renderer->renderer,
        SDL_BLENDMODE_BLEND
    );
    SDL_RenderSetLogicalSize(renderer->renderer, width, height);
    return 1;
}

int collision_renderer_initialize(CollisionRenderer *renderer)
{
    return collision_renderer_initialize_size(
        renderer,
        DEMO_BOARD_PIXEL_WIDTH,
        DEMO_BOARD_PIXEL_HEIGHT,
        0
    );
}

int collision_renderer_initialize_browser(CollisionRenderer *renderer)
{
    return collision_renderer_initialize_size(
        renderer,
        BROWSER_LOGICAL_WIDTH,
        BROWSER_LOGICAL_HEIGHT,
        1
    );
}

void collision_renderer_update_title(
    CollisionRenderer *renderer,
    const CollisionRun *run,
    int paused)
{
    if (!renderer || !renderer->window || !run) return;

    char title[WINDOW_TITLE_SIZE];
    const char *state = collision_run_state_name(run->state);

    if (run->state == COLLISION_RUN_RUNNING && paused)
        state = "PAUSED";

    snprintf(
        title,
        sizeof(title),
        "LifeLab | Eater 1 | GEN: %llu / %llu | %s | Space Pause | N Step | R Reset | Esc Quit",
        (unsigned long long)run->generation,
        (unsigned long long)run->scenario.simulation_limit,
        state
    );

    SDL_SetWindowTitle(renderer->window, title);
}

void collision_renderer_draw(
    CollisionRenderer *renderer,
    const LifeBoard *board)
{
    if (!renderer || !renderer->renderer || !board) return;

    SDL_SetRenderDrawColor(renderer->renderer, 5, 8, 15, 255);
    SDL_RenderClear(renderer->renderer);

    SDL_SetRenderDrawColor(renderer->renderer, 30, 45, 70, 255);

    for (int y = 0; y < LIFE_HEIGHT; ++y)
    {
        for (int x = 0; x < LIFE_WIDTH; ++x)
        {
            SDL_RenderDrawPoint(
                renderer->renderer,
                x * DEMO_CELL_SIZE + DEMO_CELL_SIZE / 2,
                y * DEMO_CELL_SIZE + DEMO_CELL_SIZE / 2
            );
        }
    }

    SDL_SetRenderDrawColor(renderer->renderer, 255, 80, 120, 255);

    for (int y = 1; y <= LIFE_HEIGHT; ++y)
    {
        for (int x = 1; x <= LIFE_WIDTH; ++x)
        {
            if (!board->cells[y][x]) continue;

            SDL_Rect cell =
            {
                (x - 1) * DEMO_CELL_SIZE,
                (y - 1) * DEMO_CELL_SIZE,
                DEMO_CELL_SIZE,
                DEMO_CELL_SIZE
            };

            SDL_RenderFillRect(renderer->renderer, &cell);
        }
    }

    SDL_RenderPresent(renderer->renderer);
}

static const BitmapGlyph bitmap_glyphs[] =
{
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}},
    {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},
    {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 17, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {':', {0, 4, 4, 0, 4, 4, 0}},
    {'/', {1, 2, 4, 4, 8, 16, 16}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
    {',', {0, 0, 0, 0, 4, 4, 8}},
    {'[', {14, 8, 8, 8, 8, 8, 14}},
    {']', {14, 2, 2, 2, 2, 2, 14}},
    {'=', {0, 0, 31, 0, 31, 0, 0}},
    {'.', {0, 0, 0, 0, 0, 4, 4}}
};

static const uint8_t *collision_renderer_glyph(char character)
{
    for (size_t i = 0;
         i < sizeof(bitmap_glyphs) / sizeof(bitmap_glyphs[0]);
         ++i)
    {
        if (bitmap_glyphs[i].character == character)
            return bitmap_glyphs[i].rows;
    }

    return NULL;
}

static void collision_renderer_text(
    SDL_Renderer *renderer,
    int x,
    int y,
    int scale,
    const char *text,
    SDL_Color color)
{
    SDL_SetRenderDrawColor(
        renderer,
        color.r,
        color.g,
        color.b,
        color.a
    );

    for (size_t character_index = 0;
         text[character_index] != '\0';
         ++character_index)
    {
        char character = text[character_index];
        const uint8_t *rows = collision_renderer_glyph(character);

        if (rows)
        {
            for (int row = 0; row < 7; ++row)
            {
                for (int column = 0; column < 5; ++column)
                {
                    if (!(rows[row] & (1U << (4 - column)))) continue;

                    SDL_Rect pixel =
                    {
                        x + (int)character_index * 6 * scale +
                            column * scale,
                        y + row * scale,
                        scale,
                        scale
                    };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
    }
}

static void collision_renderer_labelize(
    char *destination,
    size_t destination_size,
    const char *source)
{
    if (destination_size == 0) return;

    size_t index = 0;

    while (source[index] != '\0' && index + 1 < destination_size)
    {
        destination[index] = source[index] == '_' ? ' ' : source[index];
        index += 1;
    }

    destination[index] = '\0';
}

void collision_renderer_draw_browser_loading(CollisionRenderer *renderer)
{
    if (!renderer || !renderer->renderer || !renderer->window) return;

    SDL_SetWindowTitle(renderer->window, "LifeLab | Searching collisions...");
    SDL_SetRenderDrawColor(renderer->renderer, 8, 15, 29, 255);
    SDL_RenderClear(renderer->renderer);

    SDL_SetRenderDrawColor(renderer->renderer, 17, 24, 39, 255);
    SDL_Rect loading_card = {240, 276, 600, 208};
    SDL_RenderFillRect(renderer->renderer, &loading_card);
    SDL_SetRenderDrawColor(renderer->renderer, 51, 65, 85, 255);
    SDL_RenderDrawRect(renderer->renderer, &loading_card);

    collision_renderer_text(
        renderer->renderer,
        498,
        330,
        2,
        "LIFELAB",
        (SDL_Color){34, 197, 94, 255}
    );
    collision_renderer_text(
        renderer->renderer,
        420,
        366,
        2,
        "SEARCHING COLLISIONS",
        (SDL_Color){248, 250, 252, 255}
    );
    collision_renderer_text(
        renderer->renderer,
        453,
        400,
        1,
        "BUILDING EATER 1 CASE CATALOG",
        (SDL_Color){148, 163, 184, 255}
    );
    SDL_RenderPresent(renderer->renderer);
    SDL_PumpEvents();
}

static int collision_renderer_signal_has_cell(
    SignalState signal,
    int x,
    int y)
{
    for (int cell_index = 0;
         cell_index < GLIDER_CELL_COUNT;
         ++cell_index)
    {
        WorldAnchor cell;

        if (glider_get_live_cell(signal, cell_index, &cell) &&
            life_wrap_coordinate(cell.x, LIFE_WIDTH) == x &&
            life_wrap_coordinate(cell.y, LIFE_HEIGHT) == y)
            return 1;
    }

    return 0;
}

static int collision_renderer_inside_pattern(
    const CollisionReplay *replay,
    int x,
    int y)
{
    int64_t relative_x = x - replay->scenario.pattern_origin.x;
    int64_t relative_y = y - replay->scenario.pattern_origin.y;

    return relative_x >= 0 &&
           relative_x < replay->scenario.pattern->width &&
           relative_y >= 0 &&
           relative_y < replay->scenario.pattern->height;
}

static int collision_renderer_expected_pattern_cell(
    const CollisionReplay *replay,
    int x,
    int y)
{
    int relative_x = (int)(x - replay->scenario.pattern_origin.x);
    int relative_y = (int)(y - replay->scenario.pattern_origin.y);

    return pattern_has_cell(
        replay->scenario.pattern,
        relative_x,
        relative_y
    );
}

static BrowserViewport collision_renderer_viewport(
    const CollisionReplay *replay)
{
    int64_t bbox_width = replay->reaction_bbox.max_x -
        replay->reaction_bbox.min_x + 1;
    int64_t bbox_height = replay->reaction_bbox.max_y -
        replay->reaction_bbox.min_y + 1;
    int required_columns = (int)bbox_width +
        2 * BROWSER_VIEW_PADDING_CELLS;
    int required_rows = (int)bbox_height +
        2 * BROWSER_VIEW_PADDING_CELLS;
    int horizontal_cell_size = BROWSER_VIEW_WIDTH / required_columns;
    int vertical_cell_size = BROWSER_VIEW_HEIGHT / required_rows;
    int cell_size = horizontal_cell_size < vertical_cell_size
        ? horizontal_cell_size
        : vertical_cell_size;

    if (cell_size < 1) cell_size = 1;
    if (cell_size > BROWSER_MAX_CELL_SIZE)
        cell_size = BROWSER_MAX_CELL_SIZE;

    int columns = BROWSER_VIEW_WIDTH / cell_size;
    int rows = BROWSER_VIEW_HEIGHT / cell_size;

    if (columns > LIFE_WIDTH) columns = LIFE_WIDTH;
    if (rows > LIFE_HEIGHT) rows = LIFE_HEIGHT;

    int width = columns * cell_size;
    int height = rows * cell_size;
    int64_t center_x = (replay->reaction_bbox.min_x +
        replay->reaction_bbox.max_x) / 2;
    int64_t center_y = (replay->reaction_bbox.min_y +
        replay->reaction_bbox.max_y) / 2;
    int64_t origin_x = center_x - columns / 2;
    int64_t origin_y = center_y - rows / 2;

    if (origin_x < 0) origin_x = 0;
    if (origin_y < 0) origin_y = 0;
    if (origin_x + columns > LIFE_WIDTH)
        origin_x = LIFE_WIDTH - columns;
    if (origin_y + rows > LIFE_HEIGHT)
        origin_y = LIFE_HEIGHT - rows;

    return (BrowserViewport)
    {
        {
            origin_x,
            origin_y
        },
        BROWSER_VIEW_X + (BROWSER_VIEW_WIDTH - width) / 2,
        BROWSER_VIEW_Y + (BROWSER_VIEW_HEIGHT - height) / 2,
        width,
        height,
        cell_size,
        columns,
        rows
    };
}

static int collision_renderer_world_offset(
    int64_t coordinate,
    int64_t view_origin,
    int world_size)
{
    int wrapped_coordinate = life_wrap_coordinate(coordinate, world_size);
    int wrapped_origin = life_wrap_coordinate(view_origin, world_size);
    int offset = wrapped_coordinate - wrapped_origin;

    if (offset < 0) offset += world_size;
    return offset;
}

static int collision_renderer_anchor_pixel(
    WorldAnchor anchor,
    const BrowserViewport *viewport,
    int *pixel_x,
    int *pixel_y)
{
    int column = collision_renderer_world_offset(
        anchor.x,
        viewport->origin.x,
        LIFE_WIDTH
    );
    int row = collision_renderer_world_offset(
        anchor.y,
        viewport->origin.y,
        LIFE_HEIGHT
    );

    if (column >= viewport->columns || row >= viewport->rows)
        return 0;

    *pixel_x = viewport->x + column * viewport->cell_size +
        viewport->cell_size / 2;
    *pixel_y = viewport->y + row * viewport->cell_size +
        viewport->cell_size / 2;
    return 1;
}

static void collision_renderer_draw_cross(
    SDL_Renderer *renderer,
    int x,
    int y,
    int radius,
    SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x - radius, y, x + radius, y);
    SDL_RenderDrawLine(renderer, x, y - radius, x, y + radius);
    SDL_RenderDrawLine(
        renderer,
        x - radius + 1,
        y - 1,
        x + radius - 1,
        y - 1
    );
    SDL_RenderDrawLine(
        renderer,
        x - 1,
        y - radius + 1,
        x - 1,
        y + radius - 1
    );
}

static void collision_renderer_draw_browser_grid(
    SDL_Renderer *renderer,
    const BrowserViewport *viewport)
{
    SDL_SetRenderDrawColor(renderer, 30, 41, 59, 210);
    int grid_stride = viewport->cell_size >= 4 ? 1 : 4;

    for (int column = 0;
         column <= viewport->columns;
         column += grid_stride)
    {
        int x = viewport->x + column * viewport->cell_size;
        SDL_RenderDrawLine(
            renderer,
            x,
            viewport->y,
            x,
            viewport->y + viewport->height
        );
    }

    for (int row = 0; row <= viewport->rows; row += grid_stride)
    {
        int y = viewport->y + row * viewport->cell_size;
        SDL_RenderDrawLine(
            renderer,
            viewport->x,
            y,
            viewport->x + viewport->width,
            y
        );
    }
}

static size_t collision_renderer_active_outputs(
    const CollisionReplay *replay,
    const LifeBoard *board,
    uint64_t generation,
    SignalState active[OUTCOME_MAX_OUTPUT_GLIDERS])
{
    size_t active_count = 0;

    for (size_t output_index = 0;
         output_index < replay->search_case->output_glider_count;
         ++output_index)
    {
        SignalState signal;

        if (!collision_replay_output_at_generation(
                replay,
                output_index,
                generation,
                &signal) ||
            glider_verify(board, signal, generation) != VERIFY_OK)
            continue;

        active[active_count] = signal;
        active_count += 1;
    }

    return active_count;
}

static void collision_renderer_draw_browser_board(
    SDL_Renderer *renderer,
    const CollisionBrowser *browser)
{
    const CollisionReplay *replay = &browser->replay;
    const CollisionSearchCase *search_case = replay->search_case;
    const LifeBoard *board = collision_replay_board(replay);
    uint64_t generation = collision_replay_generation(replay);
    SignalState input = collision_replay_expected_input(replay, generation);
    SignalState active_outputs[OUTCOME_MAX_OUTPUT_GLIDERS];
    size_t active_output_count = collision_renderer_active_outputs(
        replay,
        board,
        generation,
        active_outputs
    );
    SDL_Color pattern_color = {34, 197, 94, 255};
    SDL_Color input_color = {56, 189, 248, 255};
    SDL_Color reaction_color = {245, 158, 11, 255};
    SDL_Color output_color = {167, 139, 250, 255};
    SDL_Color debris_color = {239, 68, 68, 255};
    BrowserViewport viewport = collision_renderer_viewport(replay);
    int marker_radius = viewport.cell_size / 2 + 2;
    int cell_inset = viewport.cell_size >= 8
        ? 2
        : (viewport.cell_size >= 3 ? 1 : 0);

    if (marker_radius < 4) marker_radius = 4;
    if (marker_radius > 9) marker_radius = 9;

    SDL_Rect board_card = {20, 88, 716, 548};
    SDL_Rect board_clip =
    {
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    };

    SDL_SetRenderDrawColor(renderer, 17, 24, 39, 255);
    SDL_RenderFillRect(renderer, &board_card);
    SDL_SetRenderDrawColor(renderer, 51, 65, 85, 255);
    SDL_RenderDrawRect(renderer, &board_card);
    SDL_SetRenderDrawColor(renderer, 11, 18, 32, 255);
    SDL_RenderFillRect(renderer, &board_clip);
    SDL_RenderSetClipRect(renderer, &board_clip);

    collision_renderer_draw_browser_grid(renderer, &viewport);

    for (int row = 0; row < viewport.rows; ++row)
    {
        int world_y = life_wrap_coordinate(
            viewport.origin.y + row,
            LIFE_HEIGHT
        );

        for (int column = 0; column < viewport.columns; ++column)
        {
            int world_x = life_wrap_coordinate(
                viewport.origin.x + column,
                LIFE_WIDTH
            );

            if (!board->cells[world_y + 1][world_x + 1]) continue;

            SDL_Color color = reaction_color;
            int classified = 0;

            for (size_t output_index = 0;
                 output_index < active_output_count;
                 ++output_index)
            {
                if (collision_renderer_signal_has_cell(
                        active_outputs[output_index],
                        world_x,
                        world_y))
                {
                    color = output_color;
                    classified = 1;
                    break;
                }
            }

            if (!classified &&
                generation < search_case->reaction_start_generation &&
                collision_renderer_signal_has_cell(input, world_x, world_y))
            {
                color = input_color;
                classified = 1;
            }

            if (!classified &&
                ((generation == search_case->terminal_generation &&
                  collision_renderer_inside_pattern(
                      replay,
                      world_x,
                      world_y)) ||
                 collision_renderer_expected_pattern_cell(
                     replay,
                     world_x,
                     world_y)))
            {
                color = pattern_color;
                classified = 1;
            }

            if (!classified &&
                generation == search_case->terminal_generation)
                color = debris_color;

            SDL_SetRenderDrawColor(
                renderer,
                color.r,
                color.g,
                color.b,
                color.a
            );
            SDL_Rect cell =
            {
                viewport.x + column * viewport.cell_size + cell_inset,
                viewport.y + row * viewport.cell_size + cell_inset,
                viewport.cell_size - 2 * cell_inset,
                viewport.cell_size - 2 * cell_inset
            };
            SDL_RenderFillRect(renderer, &cell);
        }
    }

    if (!browser->overlays_visible)
    {
        SDL_RenderSetClipRect(renderer, NULL);
        return;
    }

    uint64_t trajectory_generation = generation <
        search_case->reaction_start_generation
            ? generation
            : search_case->reaction_start_generation;
    SignalState trajectory_input = collision_replay_expected_input(
        replay,
        trajectory_generation
    );
    int input_start_x;
    int input_start_y;
    int input_current_x;
    int input_current_y;

    if (collision_renderer_anchor_pixel(
            replay->scenario.incoming_glider.anchor,
            &viewport,
            &input_start_x,
            &input_start_y) &&
        collision_renderer_anchor_pixel(
            trajectory_input.anchor,
            &viewport,
            &input_current_x,
            &input_current_y))
    {
        SDL_SetRenderDrawColor(renderer, 56, 189, 248, 190);
        SDL_RenderDrawLine(
            renderer,
            input_start_x,
            input_start_y,
            input_current_x,
            input_current_y
        );
        SDL_RenderDrawLine(
            renderer,
            input_start_x + 1,
            input_start_y,
            input_current_x + 1,
            input_current_y
        );
        collision_renderer_draw_cross(
            renderer,
            input_current_x,
            input_current_y,
            marker_radius,
            input_color
        );
    }

    WorldAnchor pattern_center =
    {
        replay->scenario.pattern_origin.x +
            replay->scenario.pattern->width / 2,
        replay->scenario.pattern_origin.y +
            replay->scenario.pattern->height / 2
    };

    for (size_t output_index = 0;
         output_index < search_case->output_glider_count;
         ++output_index)
    {
        SignalState output = search_case->output_gliders[output_index];
        int pattern_x;
        int pattern_y;
        int output_x;
        int output_y;

        if (collision_renderer_anchor_pixel(
                pattern_center,
                &viewport,
                &pattern_x,
                &pattern_y) &&
            collision_renderer_anchor_pixel(
                output.anchor,
                &viewport,
                &output_x,
                &output_y))
        {
            SDL_SetRenderDrawColor(renderer, 167, 139, 250, 180);
            SDL_RenderDrawLine(
                renderer,
                pattern_x,
                pattern_y,
                output_x,
                output_y
            );
            collision_renderer_draw_cross(
                renderer,
                output_x,
                output_y,
                marker_radius,
                output_color
            );
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);
}

static void collision_renderer_legend_row(
    SDL_Renderer *renderer,
    int x,
    int y,
    SDL_Color color,
    const char *label)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect swatch = {x, y, 9, 9};
    SDL_RenderFillRect(renderer, &swatch);
    collision_renderer_text(
        renderer,
        x + 16,
        y + 1,
        1,
        label,
        (SDL_Color){248, 250, 252, 255}
    );
}

static void collision_renderer_draw_browser_header(
    SDL_Renderer *renderer,
    const CollisionBrowser *browser)
{
    SDL_Color foreground = {248, 250, 252, 255};
    SDL_Color muted = {148, 163, 184, 255};
    SDL_Color accent = {34, 197, 94, 255};
    char line[96];
    SDL_Rect header = {20, 20, 1040, 52};
    SDL_SetRenderDrawColor(renderer, 17, 24, 39, 255);
    SDL_RenderFillRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, 51, 65, 85, 255);
    SDL_RenderDrawRect(renderer, &header);

    collision_renderer_text(renderer, 36, 34, 2, "LIFELAB", accent);
    collision_renderer_text(
        renderer,
        134,
        39,
        1,
        "COLLISION BROWSER",
        muted
    );

    size_t match_count = collision_browser_matching_count(browser);
    size_t match_position = collision_browser_matching_position(browser);
    snprintf(
        line,
        sizeof(line),
        "CASE %zu / %zu",
        match_position,
        match_count
    );
    collision_renderer_text(renderer, 386, 34, 2, line, foreground);
    snprintf(
        line,
        sizeof(line),
        "FILTER: %s",
        collision_browser_filter_name(browser->filter)
    );
    collision_renderer_text(renderer, 650, 39, 1, line, accent);
    collision_renderer_text(
        renderer,
        836,
        39,
        1,
        browser->overlays_visible ? "OVERLAY ON" : "OVERLAY OFF",
        muted
    );

    collision_renderer_text(
        renderer,
        966,
        39,
        1,
        browser->paused ? "PAUSED" : "PLAYING",
        browser->paused ? muted : accent
    );
}

static void collision_renderer_draw_browser_inspector(
    SDL_Renderer *renderer,
    const CollisionBrowser *browser)
{
    SDL_Color foreground = {248, 250, 252, 255};
    SDL_Color muted = {148, 163, 184, 255};
    SDL_Color accent = {34, 197, 94, 255};
    SDL_Rect inspector =
    {
        BROWSER_INSPECTOR_X,
        BROWSER_INSPECTOR_Y,
        BROWSER_INSPECTOR_WIDTH,
        BROWSER_INSPECTOR_HEIGHT
    };
    int x = BROWSER_INSPECTOR_X + 20;
    char line[96];

    SDL_SetRenderDrawColor(renderer, 17, 24, 39, 255);
    SDL_RenderFillRect(renderer, &inspector);
    SDL_SetRenderDrawColor(renderer, 51, 65, 85, 255);
    SDL_RenderDrawRect(renderer, &inspector);

    const CollisionSearchCase *search_case =
        collision_browser_current_case(browser);

    if (!search_case)
    {
        collision_renderer_text(renderer, x, 116, 1, "FILTER", muted);
        collision_renderer_text(
            renderer,
            x,
            140,
            2,
            "NO CASES",
            (SDL_Color){239, 68, 68, 255}
        );
        collision_renderer_text(renderer, x, 174, 2, "PRESS F TO CONTINUE", foreground);
        return;
    }

    collision_renderer_text(renderer, x, 112, 1, "COLLISION", muted);
    collision_renderer_labelize(
        line,
        sizeof(line),
        collision_outcome_name(search_case->outcome)
    );
    collision_renderer_text(renderer, x, 132, 2, line, accent);
    collision_renderer_labelize(
        line,
        sizeof(line),
        collision_pattern_state_name(search_case->pattern_state)
    );
    collision_renderer_text(renderer, x, 158, 1, line, foreground);

    SDL_SetRenderDrawColor(renderer, 51, 65, 85, 255);
    SDL_RenderDrawLine(renderer, x, 184, x + 264, 184);

    collision_renderer_text(renderer, x, 204, 1, "INPUT", muted);
    snprintf(
        line,
        sizeof(line),
        "EATER 1 / %s / %s",
        glider_direction_name(search_case->direction),
        glider_phase_name(search_case->phase)
    );
    collision_renderer_text(renderer, x, 224, 2, line, foreground);
    snprintf(
        line,
        sizeof(line),
        "OFFSET: %d, %d",
        search_case->offset_x,
        search_case->offset_y
    );
    collision_renderer_text(renderer, x, 250, 2, line, foreground);

    collision_renderer_text(renderer, x, 290, 1, "REACTION", muted);
    snprintf(
        line,
        sizeof(line),
        "DURATION: %llu",
        (unsigned long long)search_case->reaction_duration
    );
    collision_renderer_text(renderer, x, 312, 2, line, foreground);
    snprintf(
        line,
        sizeof(line),
        "OUTPUTS: %zu",
        search_case->output_glider_count
    );
    collision_renderer_text(renderer, x, 338, 2, line, foreground);
    snprintf(
        line,
        sizeof(line),
        "DEBRIS: %s",
        search_case->debris_present == COLLISION_VALUE_YES ? "YES" : "NO"
    );
    collision_renderer_text(renderer, x, 364, 2, line, foreground);
    snprintf(
        line,
        sizeof(line),
        "OCCURRENCES: %zu",
        search_case->occurrences
    );
    collision_renderer_text(renderer, x, 390, 2, line, foreground);

    SDL_SetRenderDrawColor(renderer, 51, 65, 85, 255);
    SDL_RenderDrawLine(renderer, x, 420, x + 264, 420);

    collision_renderer_text(renderer, x, 440, 1, "LAYERS", muted);
    collision_renderer_legend_row(
        renderer,
        x,
        464,
        (SDL_Color){34, 197, 94, 255},
        "PATTERN"
    );
    collision_renderer_legend_row(
        renderer,
        x + 136,
        464,
        (SDL_Color){56, 189, 248, 255},
        "INPUT"
    );
    collision_renderer_legend_row(
        renderer,
        x,
        490,
        (SDL_Color){245, 158, 11, 255},
        "REACTION"
    );
    collision_renderer_legend_row(
        renderer,
        x + 136,
        490,
        (SDL_Color){167, 139, 250, 255},
        "OUTPUT"
    );
    collision_renderer_legend_row(
        renderer,
        x,
        516,
        (SDL_Color){239, 68, 68, 255},
        "DEBRIS"
    );

}

static void collision_renderer_draw_browser_empty_board(
    SDL_Renderer *renderer)
{
    SDL_Rect board_card = {20, 88, 716, 548};
    SDL_SetRenderDrawColor(renderer, 17, 24, 39, 255);
    SDL_RenderFillRect(renderer, &board_card);
    SDL_SetRenderDrawColor(renderer, 51, 65, 85, 255);
    SDL_RenderDrawRect(renderer, &board_card);
    collision_renderer_text(
        renderer,
        260,
        338,
        2,
        "NO CASES IN FILTER",
        (SDL_Color){148, 163, 184, 255}
    );
}

static void collision_renderer_draw_browser_footer(
    SDL_Renderer *renderer,
    const CollisionBrowser *browser)
{
    SDL_Color foreground = {248, 250, 252, 255};
    SDL_Color muted = {148, 163, 184, 255};
    SDL_Color accent = {34, 197, 94, 255};
    SDL_Rect footer = {20, BROWSER_FOOTER_Y, 1040, 88};
    SDL_SetRenderDrawColor(renderer, 17, 24, 39, 255);
    SDL_RenderFillRect(renderer, &footer);
    SDL_SetRenderDrawColor(renderer, 51, 65, 85, 255);
    SDL_RenderDrawRect(renderer, &footer);

    const CollisionSearchCase *search_case =
        collision_browser_current_case(browser);
    char line[96];

    if (search_case)
    {
        uint64_t generation = collision_replay_generation(&browser->replay);
        snprintf(
            line,
            sizeof(line),
            "GEN %llu / %llu",
            (unsigned long long)generation,
            (unsigned long long)search_case->terminal_generation
        );
        collision_renderer_text(renderer, 36, 670, 2, line, foreground);
        collision_renderer_text(
            renderer,
            36,
            702,
            1,
            browser->paused ? "PAUSED" : "PLAYING",
            browser->paused ? muted : accent
        );

        SDL_Rect progress_track = {220, 675, 300, 12};
        SDL_SetRenderDrawColor(renderer, 30, 41, 59, 255);
        SDL_RenderFillRect(renderer, &progress_track);

        int progress_width = search_case->terminal_generation == 0
            ? progress_track.w
            : (int)(generation * (uint64_t)progress_track.w /
                    search_case->terminal_generation);
        SDL_Rect progress =
        {
            progress_track.x,
            progress_track.y,
            progress_width,
            progress_track.h
        };
        SDL_SetRenderDrawColor(renderer, 34, 197, 94, 255);
        SDL_RenderFillRect(renderer, &progress);
        collision_renderer_text(
            renderer,
            220,
            702,
            1,
            "REPLAY PROGRESS",
            muted
        );
    }

    collision_renderer_text(
        renderer,
        558,
        668,
        2,
        "SPACE PLAY / PAUSE   N / B STEP",
        foreground
    );
    collision_renderer_text(
        renderer,
        558,
        692,
        2,
        "[ / ] CASE   R RESTART   F FILTER",
        foreground
    );
    collision_renderer_text(
        renderer,
        558,
        716,
        2,
        "O OVERLAY   ESC QUIT",
        foreground
    );
}

void collision_renderer_update_browser_title(
    CollisionRenderer *renderer,
    const CollisionBrowser *browser)
{
    if (!renderer || !renderer->window || !browser) return;

    char title[WINDOW_TITLE_SIZE];
    const CollisionSearchCase *search_case =
        collision_browser_current_case(browser);

    if (!search_case)
    {
        snprintf(
            title,
            sizeof(title),
            "LifeLab | Filter: %s | No matching cases | F Next Filter | Esc Quit",
            collision_browser_filter_name(browser->filter)
        );
    }
    else
    {
        snprintf(
            title,
            sizeof(title),
            "LifeLab | Case %zu/%zu | %s | %s | Gen %llu/%llu | %s",
            collision_browser_matching_position(browser),
            collision_browser_matching_count(browser),
            collision_browser_filter_name(browser->filter),
            collision_outcome_name(search_case->outcome),
            (unsigned long long)collision_replay_generation(
                &browser->replay
            ),
            (unsigned long long)search_case->terminal_generation,
            browser->paused ? "PAUSED" : "PLAYING"
        );
    }

    SDL_SetWindowTitle(renderer->window, title);
}

void collision_renderer_draw_browser(
    CollisionRenderer *renderer,
    const CollisionBrowser *browser)
{
    if (!renderer || !renderer->renderer || !browser) return;

    SDL_SetRenderDrawColor(renderer->renderer, 8, 15, 29, 255);
    SDL_RenderClear(renderer->renderer);

    collision_renderer_draw_browser_header(renderer->renderer, browser);

    if (browser->replay_ready && collision_replay_board(&browser->replay))
        collision_renderer_draw_browser_board(
            renderer->renderer,
            browser
        );
    else
        collision_renderer_draw_browser_empty_board(renderer->renderer);

    collision_renderer_draw_browser_inspector(renderer->renderer, browser);
    collision_renderer_draw_browser_footer(renderer->renderer, browser);
    SDL_RenderPresent(renderer->renderer);
}

void collision_renderer_destroy(CollisionRenderer *renderer)
{
    if (!renderer) return;

    if (renderer->renderer)
        SDL_DestroyRenderer(renderer->renderer);

    if (renderer->window)
        SDL_DestroyWindow(renderer->window);

    renderer->renderer = NULL;
    renderer->window = NULL;
    renderer->logical_width = 0;
    renderer->logical_height = 0;
    SDL_Quit();
}
