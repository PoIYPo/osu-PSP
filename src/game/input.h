// Player input, in playfield coordinates (0..512, 0..384).
#pragma once

#include "core/types.h"

namespace osu {

struct InputState {
    Vec2 cursor;          // where the player is pointing
    bool pressed = false; // action button held this frame
};

}  // namespace osu
