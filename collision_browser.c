#include "collision_browser.h"

#include <stdint.h>
#include <string.h>

static int collision_browser_case_matches_filter(
    const CollisionSearchCase *search_case,
    CollisionBrowserFilter filter)
{
    switch (filter)
    {
        case COLLISION_FILTER_ALL:
            return 1;
        case COLLISION_FILTER_ABSORB:
            return search_case->outcome == COLLISION_OUTCOME_ABSORB;
        case COLLISION_FILTER_REFLECT:
            return search_case->outcome == COLLISION_OUTCOME_REFLECT;
        case COLLISION_FILTER_GLIDER_OUTPUT:
            return search_case->output_glider_count > 0;
        case COLLISION_FILTER_DEBRIS:
            return search_case->debris_present == COLLISION_VALUE_YES;
        case COLLISION_FILTER_PATTERN_CHANGED:
            return search_case->pattern_state == COLLISION_PATTERN_CHANGED;
        case COLLISION_FILTER_COUNT:
        default:
            return 0;
    }
}

static int collision_browser_load_case(
    CollisionBrowser *browser,
    size_t case_index)
{
    if (!browser || case_index >= browser->report.case_count) return 0;

    CollisionReplay next_replay;

    if (!collision_replay_initialize(
            &next_replay,
            &browser->report.cases[case_index]))
        return 0;

    collision_replay_destroy(&browser->replay);
    browser->replay = next_replay;
    browser->selected_case_index = case_index;
    browser->replay_ready = 1;
    browser->paused = 0;
    browser->last_advance_ms = 0;
    return 1;
}

static int collision_browser_select_first_match(
    CollisionBrowser *browser)
{
    for (size_t i = 0; i < browser->report.case_count; ++i)
    {
        if (collision_browser_case_matches_filter(
                &browser->report.cases[i],
                browser->filter))
            return collision_browser_load_case(browser, i);
    }

    collision_replay_destroy(&browser->replay);
    browser->selected_case_index = SIZE_MAX;
    browser->replay_ready = 0;
    browser->paused = 1;
    browser->last_advance_ms = 0;
    return 1;
}

static int collision_browser_select_relative(
    CollisionBrowser *browser,
    int direction)
{
    if (!browser || browser->report.case_count == 0) return 1;

    size_t count = browser->report.case_count;
    size_t start = browser->selected_case_index == SIZE_MAX
        ? 0
        : browser->selected_case_index;

    for (size_t step = 1; step <= count; ++step)
    {
        size_t index;

        if (direction > 0)
            index = (start + step) % count;
        else
            index = (start + count - (step % count)) % count;

        if (collision_browser_case_matches_filter(
                &browser->report.cases[index],
                browser->filter))
            return collision_browser_load_case(browser, index);
    }

    return 1;
}

int collision_browser_initialize(CollisionBrowser *browser)
{
    if (!browser) return 0;

    memset(browser, 0, sizeof(*browser));
    browser->selected_case_index = SIZE_MAX;
    browser->filter = COLLISION_FILTER_ALL;
    browser->overlays_visible = 1;

    if (!collision_search_eater1(&browser->report) ||
        browser->report.case_count == 0)
    {
        collision_browser_destroy(browser);
        return 0;
    }

    if (!collision_browser_select_first_match(browser))
    {
        collision_browser_destroy(browser);
        return 0;
    }

    return 1;
}

void collision_browser_destroy(CollisionBrowser *browser)
{
    if (!browser) return;

    collision_replay_destroy(&browser->replay);
    collision_search_report_destroy(&browser->report);
    memset(browser, 0, sizeof(*browser));
}

int collision_browser_handle_action(
    CollisionBrowser *browser,
    CollisionBrowserAction action)
{
    if (!browser) return 0;

    switch (action)
    {
        case COLLISION_BROWSER_TOGGLE_PLAY:
            if (!browser->replay_ready) return 1;

            if (browser->paused &&
                browser->replay.current_frame + 1 >=
                    browser->replay.frame_count)
                collision_replay_restart(&browser->replay);

            browser->paused = !browser->paused;
            browser->last_advance_ms = 0;
            return 1;
        case COLLISION_BROWSER_NEXT_GENERATION:
            browser->paused = 1;
            collision_replay_next(&browser->replay);
            return 1;
        case COLLISION_BROWSER_PREVIOUS_GENERATION:
            browser->paused = 1;
            collision_replay_previous(&browser->replay);
            return 1;
        case COLLISION_BROWSER_RESTART:
            collision_replay_restart(&browser->replay);
            browser->paused = 1;
            browser->last_advance_ms = 0;
            return 1;
        case COLLISION_BROWSER_PREVIOUS_CASE:
            return collision_browser_select_relative(browser, -1);
        case COLLISION_BROWSER_NEXT_CASE:
            return collision_browser_select_relative(browser, 1);
        case COLLISION_BROWSER_NEXT_FILTER:
            browser->filter = (CollisionBrowserFilter)(
                (browser->filter + 1) % COLLISION_FILTER_COUNT
            );
            return collision_browser_select_first_match(browser);
        case COLLISION_BROWSER_TOGGLE_OVERLAYS:
            browser->overlays_visible = !browser->overlays_visible;
            return 1;
        default:
            return 1;
    }
}

void collision_browser_update(CollisionBrowser *browser, uint32_t now_ms)
{
    if (!browser || !browser->replay_ready || browser->paused) return;

    if (browser->last_advance_ms == 0)
    {
        browser->last_advance_ms = now_ms;
        return;
    }

    if (now_ms - browser->last_advance_ms <
        COLLISION_BROWSER_FRAME_INTERVAL_MS)
        return;

    browser->last_advance_ms = now_ms;

    if (!collision_replay_next(&browser->replay))
        browser->paused = 1;
}

const CollisionSearchCase *collision_browser_current_case(
    const CollisionBrowser *browser)
{
    if (!browser || !browser->replay_ready ||
        browser->selected_case_index >= browser->report.case_count)
        return NULL;

    return &browser->report.cases[browser->selected_case_index];
}

size_t collision_browser_matching_count(const CollisionBrowser *browser)
{
    if (!browser) return 0;

    size_t count = 0;

    for (size_t i = 0; i < browser->report.case_count; ++i)
    {
        if (collision_browser_case_matches_filter(
                &browser->report.cases[i],
                browser->filter))
            count += 1;
    }

    return count;
}

size_t collision_browser_matching_position(const CollisionBrowser *browser)
{
    if (!browser || !browser->replay_ready) return 0;

    size_t position = 0;

    for (size_t i = 0; i <= browser->selected_case_index; ++i)
    {
        if (collision_browser_case_matches_filter(
                &browser->report.cases[i],
                browser->filter))
            position += 1;
    }

    return position;
}

const char *collision_browser_filter_name(CollisionBrowserFilter filter)
{
    switch (filter)
    {
        case COLLISION_FILTER_ALL:
            return "ALL";
        case COLLISION_FILTER_ABSORB:
            return "ABSORB";
        case COLLISION_FILTER_REFLECT:
            return "REFLECT";
        case COLLISION_FILTER_GLIDER_OUTPUT:
            return "GLIDER OUTPUT";
        case COLLISION_FILTER_DEBRIS:
            return "DEBRIS";
        case COLLISION_FILTER_PATTERN_CHANGED:
            return "PATTERN CHANGED";
        case COLLISION_FILTER_COUNT:
        default:
            return "UNKNOWN";
    }
}
