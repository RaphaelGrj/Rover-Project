#pragma once

#include <Adafruit_GFX.h>
#include "EyeState.h"

// Pure rendering: draws two white rounded-rect eyes with a black iris
// and a thin cyan/rose glitch contour + flicker lines onto any
// Adafruit_GFX target, based only on the given EyeState. No persistent
// timers/state -- each call independently rolls its own flicker, which
// is what makes the glitch read as random rather than a fixed offset
// (see EyeRenderer.cpp for why the earlier RGB-split-fill version was
// replaced after the first real-hardware test looked like a color mess
// instead of a subtle effect). Originally ported from Lumi's
// eye-drawing technique (see PROGRESS.md for the survey), rewritten as
// a standalone class instead of a loop body tied to Lumi's own globals.
namespace EyeRenderer {
void draw(Adafruit_GFX& gfx, const EyeState& state);
}
