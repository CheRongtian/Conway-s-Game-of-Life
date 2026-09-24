#ifndef CONWAY_RENDER_H
#define CONWAY_RENDER_H

#include "collision.h"

#include <SDL.h>

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
} CollisionRenderer;

int collision_renderer_initialize(CollisionRenderer *renderer);
void collision_renderer_update_title(
    CollisionRenderer *renderer,
    const CollisionRun *run,
    int paused
);
void collision_renderer_draw(
    CollisionRenderer *renderer,
    const LifeBoard *board
);
void collision_renderer_destroy(CollisionRenderer *renderer);

#endif
