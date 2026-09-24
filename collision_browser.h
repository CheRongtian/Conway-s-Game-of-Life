#ifndef CONWAY_COLLISION_BROWSER_H
#define CONWAY_COLLISION_BROWSER_H

#include "collision_replay.h"
#include "collision_search.h"

#include <stddef.h>
#include <stdint.h>

#define COLLISION_BROWSER_FRAME_INTERVAL_MS 120

typedef enum
{
    COLLISION_FILTER_ALL,
    COLLISION_FILTER_ABSORB,
    COLLISION_FILTER_REFLECT,
    COLLISION_FILTER_GLIDER_OUTPUT,
    COLLISION_FILTER_DEBRIS,
    COLLISION_FILTER_PATTERN_CHANGED,
    COLLISION_FILTER_COUNT
} CollisionBrowserFilter;

typedef enum
{
    COLLISION_BROWSER_TOGGLE_PLAY,
    COLLISION_BROWSER_NEXT_GENERATION,
    COLLISION_BROWSER_PREVIOUS_GENERATION,
    COLLISION_BROWSER_RESTART,
    COLLISION_BROWSER_PREVIOUS_CASE,
    COLLISION_BROWSER_NEXT_CASE,
    COLLISION_BROWSER_NEXT_FILTER,
    COLLISION_BROWSER_TOGGLE_OVERLAYS
} CollisionBrowserAction;

typedef struct
{
    CollisionSearchReport report;
    CollisionReplay replay;
    size_t selected_case_index;
    CollisionBrowserFilter filter;
    int replay_ready;
    int paused;
    int overlays_visible;
    uint32_t last_advance_ms;
} CollisionBrowser;

int collision_browser_initialize(CollisionBrowser *browser);
void collision_browser_destroy(CollisionBrowser *browser);
int collision_browser_handle_action(
    CollisionBrowser *browser,
    CollisionBrowserAction action
);
void collision_browser_update(CollisionBrowser *browser, uint32_t now_ms);
const CollisionSearchCase *collision_browser_current_case(
    const CollisionBrowser *browser
);
size_t collision_browser_matching_count(const CollisionBrowser *browser);
size_t collision_browser_matching_position(const CollisionBrowser *browser);
const char *collision_browser_filter_name(CollisionBrowserFilter filter);

#endif
