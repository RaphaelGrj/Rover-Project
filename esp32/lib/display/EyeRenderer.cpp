#include "EyeRenderer.h"
#include <Arduino.h>
#include "display_config.h"

namespace {

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_MAGENTA = 0xF81F;  // "rose" -- closest RGB565 to hot pink without a 3rd draw pass
constexpr uint16_t COLOR_WHITE = 0xFFFF;

// Ceilings drawOneEye itself enforces (openness clamp, glitch offset in
// px) -- kept as named constants because EyeRenderer::draw() also needs
// them, to size a redraw region guaranteed to cover every case below
// without having to duplicate the clamp logic.
constexpr float MAX_OPENNESS = 1.3f;
constexpr int MAX_GLITCH_OFFSET_PX = 3;

// Draws one square, rounded-corner eye centered at (centerX, centerY).
// glitchIntensity>0 draws the cyan/magenta copies offset a few px apart
// before the white one on top, for a chromatic-aberration "glitch"
// look; at 0 it's a clean white eye. size is used for both width and
// height at openness=1 (a square, not the taller rectangle of a
// realistic eye -- that's the requested look, not an oversight).
void drawOneEye(Adafruit_GFX& gfx, int centerX, int centerY, int size,
                 float lookX, float lookY, float openness, float glitchIntensity) {
    int h = (int)(size * constrain(openness, 0.0f, MAX_OPENNESS));
    if (h < 2) h = 2;  // a fully-closed eye is still a thin visible line, not invisible
    int w = size;
    if (w < 2) w = 2;  // defensive: size is a compile-time-derived constant today, but
                        // this keeps a future refactor from ever handing fillRoundRect
                        // a non-positive width.
    int x = centerX - w / 2;
    int y = centerY - h / 2;
    // fillRoundRect misbehaves if the radius exceeds half of either
    // side; h shrinks a lot while blinking, so the radius must track
    // the smaller of the two dimensions, not just the (constant) width.
    int radius = min(w, h) / 4;
    radius = min(radius, min(w, h) / 2);

    int glitchOffset = (int)(constrain(glitchIntensity, 0.0f, 1.0f) * MAX_GLITCH_OFFSET_PX);
    if (glitchOffset > 0) {
        gfx.fillRoundRect(x - glitchOffset, y, w, h, radius, COLOR_CYAN);
        gfx.fillRoundRect(x + glitchOffset, y, w, h, radius, COLOR_MAGENTA);
    }
    gfx.fillRoundRect(x, y, w, h, radius, COLOR_WHITE);

    // Iris: only draw while open enough to show it -- a near-closed eye
    // shouldn't show a squished iris poking out of a thin slit.
    if (openness > 0.15f) {
        int irisW = w * 4 / 10;
        int irisH = h * 5 / 10;
        int maxOffsetX = (w - irisW) / 2;
        int maxOffsetY = (h - irisH) / 2;
        int irisX = centerX + (int)(constrain(lookX, -1.0f, 1.0f) * maxOffsetX) - irisW / 2;
        int irisY = centerY + (int)(constrain(lookY, -1.0f, 1.0f) * maxOffsetY) - irisH / 2;
        gfx.fillRoundRect(irisX, irisY, irisW, irisH, irisW / 4, COLOR_BLACK);
    }
}

// Clears exactly the region one eye can ever occupy (max openness, max
// glitch offset, plus a couple px of margin) -- sized once here, from
// the same constants drawOneEye is bound by, so it can never fall out
// of sync with what actually gets drawn inside it.
void clearEyeCell(Adafruit_GFX& gfx, int centerX, int centerY, int size) {
    int cellW = size + 2 * MAX_GLITCH_OFFSET_PX + 4;
    int cellH = (int)(size * MAX_OPENNESS) + 2 * MAX_GLITCH_OFFSET_PX + 4;
    gfx.fillRect(centerX - cellW / 2, centerY - cellH / 2, cellW, cellH, COLOR_BLACK);
}

}  // namespace

void EyeRenderer::draw(Adafruit_GFX& gfx, const EyeState& state) {
    int eyeSize = ROVER_DISPLAY_WIDTH * 36 / 100;
    int gap = ROVER_DISPLAY_WIDTH * 10 / 100;
    int centerY = ROVER_DISPLAY_HEIGHT / 2;
    int leftCenterX = ROVER_DISPLAY_WIDTH / 2 - eyeSize / 2 - gap / 2;
    int rightCenterX = ROVER_DISPLAY_WIDTH / 2 + eyeSize / 2 + gap / 2;

    // Only the two eye cells are cleared+redrawn every frame, not the
    // full screen: at ~30 FPS that's a large cut in SPI traffic versus
    // a fillScreen() each frame (nothing else is ever drawn outside
    // these cells, so leaving the rest of the panel alone is safe --
    // it was cleared once, at boot, and stays black).
    clearEyeCell(gfx, leftCenterX, centerY, eyeSize);
    clearEyeCell(gfx, rightCenterX, centerY, eyeSize);

    drawOneEye(gfx, leftCenterX, centerY, eyeSize,
               state.leftLookX, state.leftLookY, state.openness, state.glitchIntensity);
    drawOneEye(gfx, rightCenterX, centerY, eyeSize,
               state.rightLookX, state.rightLookY, state.openness, state.glitchIntensity);
}
