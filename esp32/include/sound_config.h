#pragma once

// Piezo buzzer (esp32/lib/sound/Buzzer.h). GPIO12 chosen deliberately
// -- see WIRING.md "Pins évitées volontairement" for why this deviates
// from the project's earlier "never use GPIO12" rule (strapping pin,
// flash voltage selection risk at boot): explicitly accepted by the
// user (2026-08-31) since every other GPIO on the WROOM devkit was
// already spoken for. A passive piezo buzzer's high impedance rarely
// pulls a strapping pin hard enough during the boot sampling window to
// matter in practice -- watch the boot log after wiring this for real,
// this pin is the first suspect if boot ever looks wrong.
constexpr int ROVER_PIN_BUZZER = 12;
