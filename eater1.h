#ifndef CONWAY_EATER1_H
#define CONWAY_EATER1_H

#include "glider.h"
#include "pattern.h"

#include <stddef.h>

#define EATER1_PATTERN_ID 1
#define EATER1_CELL_COUNT 7
#define EATER1_WIDTH 4
#define EATER1_HEIGHT 4
#define EATER1_REACTION_MAX_FRAMES 65
#define EATER1_REACTION_MAX_WIDTH 64
#define EATER1_REACTION_MAX_HEIGHT 64

typedef struct
{
    size_t frame_count;
    int width;
    int height;
    uint8_t cells
        [EATER1_REACTION_MAX_FRAMES]
        [EATER1_REACTION_MAX_HEIGHT]
        [EATER1_REACTION_MAX_WIDTH];
} Eater1ReactionFrames;

typedef struct
{
    SignalState input_signal;
    uint64_t reaction_start_offset;
    uint64_t restore_generation_offset;
    uint64_t reaction_duration;
    WorldBox pattern_bbox;
    WorldBox reaction_bbox;
    WorldBox safety_bbox;
    Eater1ReactionFrames reaction_frames;
} Eater1Contract;

extern const PatternSpec EATER1_PATTERN;

int eater1_get_contract(Eater1Contract *contract);
VerificationResult eater1_verify_reaction_frame(
    const LifeBoard *board,
    const Eater1Contract *contract,
    WorldAnchor origin,
    size_t frame_index
);

#endif
