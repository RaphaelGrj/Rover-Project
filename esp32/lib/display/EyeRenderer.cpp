#include "EyeRenderer.h"
#include <Arduino.h>
#include "display_config.h"

namespace {

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_MAGENTA = 0xF81F;
constexpr uint16_t COLOR_WHITE = 0xFFFF;

// Draws one eye centered at (centerX, centerY). glitchIntensity>0 draws
// the cyan/magenta copies offset a few px apart before the white one on
// top, for the chromatic-aberration "glitch" look; at 0 it's just a
// clean white eye.
void drawOneEye(Adafruit_GFX& gfx, int centerX, int centerY, int width, int height,
                 float lookX, float lookY, float openness, float glitchIntensity) {
    int h = (int)(height * constrain(openness, 0.0f, 1.3f));
    if (h < 2) h = 2;  // a fully-closed eye is still a thin visible line, not invisible
    int x = centerX - width / 2;
    int y = centerY - h / 2;
    int radius = width / 5;

    int glitchOffset = (int)(constrain(glitchIntensity, 0.0f, 1.0f) * 3.0f);
    if (glitchOffset > 0) {
        gfx.fillRoundRect(x - glitchOffset, y, width, h, radius, COLOR_CYAN);
        gfx.fillRoundRect(x + glitchOffset, y, width, h, radius, COLOR_MAGENTA);
    }
    gfx.fillRoundRect(x, y, width, h, radius, COLOR_WHITE);

    // Iris: only draw while open enough to show it -- a near-closed eye
    // shouldn't show a squished iris poking out of a thin slit.
    if (openness > 0.15f) {
        int irisW = width * 4 / 10;
        int irisH = h * 5 / 10;
        int maxOffsetX = (width - irisW) / 2;
        int maxOffsetY = (h - irisH) / 2;
        int irisX = centerX + (int)(lookX * maxOffsetX) - irisW / 2;
        int irisY = centerY + (int)(lookY * maxOffsetY) - irisH / 2;
        gfx.fillRoundRect(irisX, irisY, irisW, irisH, irisW / 4, COLOR_BLACK);
    }
}

}  // namespace

void EyeRenderer::draw(Adafruit_GFX& gfx, const EyeState& state) {
    gfx.fillScreen(COLOR_BLACK);

    int eyeWidth = ROVER_DISPLAY_WIDTH * 35 / 100;
    int eyeHeight = ROVER_DISPLAY_HEIGHT * 45 / 100;
    int gap = ROVER_DISPLAY_WIDTH * 8 / 100;
    int centerY = ROVER_DISPLAY_HEIGHT / 2;
    int leftCenterX = ROVER_DISPLAY_WIDTH / 2 - eyeWidth / 2 - gap / 2;
    int rightCenterX = ROVER_DISPLAY_WIDTH / 2 + eyeWidth / 2 + gap / 2;

    drawOneEye(gfx, leftCenterX, centerY, eyeWidth, eyeHeight,
               state.leftLookX, state.leftLookY, state.openness, state.glitchIntensity);
    drawOneEye(gfx, rightCenterX, centerY, eyeWidth, eyeHeight,
               state.rightLookX, state.rightLookY, state.openness, state.glitchIntensity);
}
