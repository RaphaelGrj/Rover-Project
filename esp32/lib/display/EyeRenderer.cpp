#include "EyeRenderer.h"
#include <Arduino.h>
#include "display_config.h"

namespace {

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_MAGENTA = 0xF81F;  // "rose" -- closest RGB565 to hot pink without a 3rd draw pass
constexpr uint16_t COLOR_WHITE = 0xFFFF;

// Ceilings drawOneEye itself enforces (openness clamp, contour offset in
// px) -- kept as named constants because EyeRenderer::draw() also needs
// them, to size a redraw region guaranteed to cover every case below
// without having to duplicate the clamp logic.
constexpr float MAX_OPENNESS = 1.3f;
// Real-hardware feedback (first physical test): the original approach
// filled three offset copies of the whole eye (cyan/magenta/white),
// which reads as a color mess on the panel, not a subtle effect. Now
// the fill is a single clean white rect; "glitch" is a thin cyan/rose
// outline (CONTOUR_OFFSET_PX/CONTOUR_THICKNESS_PX) always present at
// low intensity, plus a handful of thin flicker lines confined to the
// eye's own box -- both requested explicitly to stay "fin" (thin), with
// the eye interior staying white.
constexpr int CONTOUR_OFFSET_PX = 2;
constexpr int CONTOUR_THICKNESS_PX = 2;  // second follow-up: make the outline a bit more visible

// Draws one square, rounded-corner eye centered at (centerX, centerY).
// size is used for both width and height at openness=1 (a square, not
// the taller rectangle of a realistic eye -- that's the requested look,
// not an oversight).
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
    // /3 (rather than the original /4) reads as noticeably rounder,
    // per second-round feedback.
    int radius = min(w, h) / 3;
    radius = min(radius, min(w, h) / 2);

    // Base eye: always a clean, solid white fill -- the glitch effect
    // never tints the eye itself, only its edge/surroundings (below).
    gfx.fillRoundRect(x, y, w, h, radius, COLOR_WHITE);

    float glitch = constrain(glitchIntensity, 0.0f, 1.0f);
    if (glitch > 0.0f) {
        // Permanent contour: one cyan line traced up-left of the eye
        // outline, one rose line down-right -- a subtle chromatic-
        // aberration edge, not a fill. drawRoundRect only strokes the
        // border; CONTOUR_THICKNESS_PX concentric passes per color make
        // that single line a bit more visible (second-round feedback)
        // without turning it into a filled band.
        for (int t = 0; t < CONTOUR_THICKNESS_PX; ++t) {
            gfx.drawRoundRect(x - CONTOUR_OFFSET_PX - t, y - CONTOUR_OFFSET_PX - t, w, h, radius, COLOR_CYAN);
            gfx.drawRoundRect(x + CONTOUR_OFFSET_PX + t, y + CONTOUR_OFFSET_PX + t, w, h, radius, COLOR_MAGENTA);
        }

        // Flicker lines: short, thin (1px) segments per eye per frame,
        // each independently rolled with no stored state -- at ~30 FPS
        // that reads as random flicker rather than a static mark. Count
        // and per-slot chance both scale with glitch intensity, so
        // ALERT/ANIMATION=GLITCH (intensity 1.0) flickers noticeably
        // more than the baseline per-emotion level (both bumped up in
        // this pass per feedback: "intensifier un peu le glitch").
        // Horizontal lines are weighted 65% vs 35% vertical -- also
        // requested explicitly ("des traits horizontaux de glitch
        // occasionnellement") -- and drawn a bit longer, since a
        // classic scanline-glitch look reads mostly horizontal.
        int maxLines = 1 + (int)(glitch * 3.0f);  // 1..4
        for (int i = 0; i < maxLines; ++i) {
            if (random(0, 1000) > (long)(glitch * 550.0f)) continue;
            uint16_t lineColor = (random(0, 2) == 0) ? COLOR_CYAN : COLOR_MAGENTA;
            bool horizontal = random(0, 100) < 65;
            if (horizontal) {
                int lineLen = max(1, w / 2 + (int)random(0, max(1, w / 2)));
                lineLen = min(lineLen, w);
                int lineY = y + (int)random(0, h);
                int lineX = x + (int)random(0, w - lineLen + 1);
                gfx.drawFastHLine(lineX, lineY, lineLen, lineColor);
            } else {
                int lineLen = max(1, h / 3 + (int)random(0, max(1, h / 3)));
                lineLen = min(lineLen, h);
                int lineX = x + (int)random(0, w);
                int lineY = y + (int)random(0, h - lineLen + 1);
                gfx.drawFastVLine(lineX, lineY, lineLen, lineColor);
            }
        }
    }

    // Iris: only draw while open enough to show it -- a near-closed eye
    // shouldn't show a squished iris poking out of a thin slit. Drawn
    // last so it stays on top of the contour/flicker.
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
// contour reach, plus a couple px of margin) -- sized once here, from
// the same constants drawOneEye is bound by, so it can never fall out
// of sync with what actually gets drawn inside it.
void clearEyeCell(Adafruit_GFX& gfx, int centerX, int centerY, int size) {
    int contourReach = CONTOUR_OFFSET_PX + CONTOUR_THICKNESS_PX - 1;
    int cellW = size + 2 * contourReach + 4;
    int cellH = (int)(size * MAX_OPENNESS) + 2 * contourReach + 4;
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
