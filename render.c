#include "render.h"

#include <stdio.h>

#define CELL_SIZE 2
#define WINDOW_TITLE_SIZE 256

int collision_renderer_initialize(CollisionRenderer *renderer)
{
    if (!renderer) return 0;

    renderer->window = NULL;
    renderer->renderer = NULL;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 0;
    }

    renderer->window = SDL_CreateWindow(
        "LifeLab",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        LIFE_WIDTH * CELL_SIZE,
        LIFE_HEIGHT * CELL_SIZE,
        0
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
    return 1;
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
                x * CELL_SIZE + CELL_SIZE / 2,
                y * CELL_SIZE + CELL_SIZE / 2
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
                (x - 1) * CELL_SIZE,
                (y - 1) * CELL_SIZE,
                CELL_SIZE,
                CELL_SIZE
            };

            SDL_RenderFillRect(renderer->renderer, &cell);
        }
    }

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
    SDL_Quit();
}
