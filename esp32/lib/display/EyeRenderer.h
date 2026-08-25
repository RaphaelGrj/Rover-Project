#pragma once

#include <Adafruit_GFX.h>
#include "EyeState.h"

// Pure rendering: draws two "glitch" eyes (RGB channel-split rounded
// rects + a black iris) onto any Adafruit_GFX target, based only on the
// given EyeState. No timers, no protocol/emotion knowledge -- ported
// from Lumi's eye-drawing technique (see PROGRESS.md for the survey),
// rewritten as a standalone class instead of a loop body tied to
// Lumi's own globals.
namespace EyeRenderer {
void draw(Adafruit_GFX& gfx, const EyeState& state);
}
