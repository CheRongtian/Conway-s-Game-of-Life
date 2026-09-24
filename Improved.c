#include "collision.h"
#include "render.h"

#include <SDL.h>
#include <stdio.h>

#define DELAY_MS 10

int main(void)
{
    CollisionScenario scenario;
    static CollisionRun run;
    CollisionRenderer renderer;
    int paused = 0;

    if (!collision_build_eater1_scenario(&scenario))
    {
        fprintf(stderr, "Unable to build the Eater 1 collision scenario.\n");
        return 1;
    }

    if (!collision_run_initialize(&run, &scenario))
    {
        fprintf(stderr, "Unable to initialize the collision run.\n");
        return 1;
    }

    if (!collision_renderer_initialize(&renderer)) return 1;

    int running = 1;
    SDL_Event event;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) running = 0;

            if (event.type == SDL_KEYDOWN && !event.key.repeat)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = 0;

                if (event.key.keysym.sym == SDLK_SPACE &&
                    run.state == COLLISION_RUN_RUNNING)
                    paused = !paused;

                if (event.key.keysym.sym == SDLK_n &&
                    paused &&
                    run.state == COLLISION_RUN_RUNNING)
                    collision_run_step(&run);

                if (event.key.keysym.sym == SDLK_r)
                {
                    if (!collision_run_initialize(&run, &scenario))
                    {
                        fprintf(stderr, "Unable to reset the collision run.\n");
                        running = 0;
                    }
                    paused = 0;
                }
            }
        }

        if (!running) break;

        collision_run_observe(&run);
        collision_renderer_update_title(&renderer, &run, paused);
        collision_renderer_draw(
            &renderer,
            collision_run_board(&run)
        );

        if (!paused && run.state == COLLISION_RUN_RUNNING)
            collision_run_step(&run);

        SDL_Delay(DELAY_MS);
    }

    collision_renderer_destroy(&renderer);
    return 0;
}
