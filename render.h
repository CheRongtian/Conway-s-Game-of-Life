#ifndef CONWAY_RENDER_H
#define CONWAY_RENDER_H

#include "collision_browser.h"
#include "collision.h"

#include <SDL.h>

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int logical_width;
    int logical_height;
} CollisionRenderer;

int collision_renderer_initialize(CollisionRenderer *renderer);
int collision_renderer_initialize_browser(CollisionRenderer *renderer);
void collision_renderer_draw_browser_loading(CollisionRenderer *renderer);
void collision_renderer_update_title(
    CollisionRenderer *renderer,
    const CollisionRun *run,
    int paused
);
void collision_renderer_draw(
    CollisionRenderer *renderer,
    const LifeBoard *board
);
void collision_renderer_update_browser_title(
    CollisionRenderer *renderer,
    const CollisionBrowser *browser
);
void collision_renderer_draw_browser(
    CollisionRenderer *renderer,
    const CollisionBrowser *browser
);
void collision_renderer_destroy(CollisionRenderer *renderer);

#endif
