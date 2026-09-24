#include "collision.h"
#include "collision_browser.h"
#include "collision_search.h"
#include "render.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define DELAY_MS 10

static int run_collision_search(void)
{
    CollisionSearchReport report;

    if (!collision_search_eater1(&report))
    {
        fprintf(stderr, "Unable to complete the collision search.\n");
        return 1;
    }

    collision_search_report_print(&report);
    collision_search_report_destroy(&report);
    return 0;
}

static int run_collision_demo(void)
{
    CollisionScenario scenario;
    static CollisionRun run;
    CollisionRenderer renderer;
    int paused = 0;
    int result_reported = 0;

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
                    result_reported = 0;
                }
            }
        }

        if (!running) break;

        collision_run_observe(&run);

        const CollisionResult *result = collision_run_result(&run);

        if (result && !result_reported)
        {
            collision_result_print(result);
            result_reported = 1;
        }

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

static int run_collision_browser(void)
{
    CollisionBrowser browser;
    CollisionRenderer renderer;

    if (!collision_renderer_initialize_browser(&renderer)) return 1;

    collision_renderer_draw_browser_loading(&renderer);

    if (!collision_browser_initialize(&browser))
    {
        fprintf(stderr, "Unable to initialize the collision browser.\n");
        collision_renderer_destroy(&renderer);
        return 1;
    }

    int running = 1;
    SDL_Event event;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) running = 0;

            if (event.type != SDL_KEYDOWN || event.key.repeat) continue;

            CollisionBrowserAction action;
            int has_action = 1;

            switch (event.key.keysym.sym)
            {
                case SDLK_ESCAPE:
                    running = 0;
                    has_action = 0;
                    break;
                case SDLK_SPACE:
                    action = COLLISION_BROWSER_TOGGLE_PLAY;
                    break;
                case SDLK_n:
                    action = COLLISION_BROWSER_NEXT_GENERATION;
                    break;
                case SDLK_b:
                    action = COLLISION_BROWSER_PREVIOUS_GENERATION;
                    break;
                case SDLK_r:
                    action = COLLISION_BROWSER_RESTART;
                    break;
                case SDLK_LEFTBRACKET:
                    action = COLLISION_BROWSER_PREVIOUS_CASE;
                    break;
                case SDLK_RIGHTBRACKET:
                    action = COLLISION_BROWSER_NEXT_CASE;
                    break;
                case SDLK_f:
                    action = COLLISION_BROWSER_NEXT_FILTER;
                    break;
                case SDLK_o:
                    action = COLLISION_BROWSER_TOGGLE_OVERLAYS;
                    break;
                default:
                    has_action = 0;
                    break;
            }

            if (has_action &&
                !collision_browser_handle_action(&browser, action))
            {
                fprintf(stderr, "Unable to update the collision browser.\n");
                running = 0;
            }
        }

        if (!running) break;

        collision_browser_update(&browser, SDL_GetTicks());
        collision_renderer_update_browser_title(&renderer, &browser);
        collision_renderer_draw_browser(&renderer, &browser);
        SDL_Delay(DELAY_MS);
    }

    collision_renderer_destroy(&renderer);
    collision_browser_destroy(&browser);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1) return run_collision_browser();

    if (argc == 2 && strcmp(argv[1], "--search") == 0)
        return run_collision_search();

    if (argc == 2 && strcmp(argv[1], "--demo") == 0)
        return run_collision_demo();

    fprintf(stderr, "Usage: %s [--search | --demo]\n", argv[0]);
    return 1;
}
