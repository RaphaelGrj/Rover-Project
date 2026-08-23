#pragma once

#include <esp_task_wdt.h>

// Hardware watchdog: catches a hung firmware loop, independent of the
// heartbeat timeout which only catches loss of Pi communication
// (ARCHITECTURE_AND_ROADMAP.md section 11: "watchdog" vs
// "communication timeout" are two separate safety layers).
namespace RoverWatchdog {

constexpr uint32_t TIMEOUT_S = 3;

inline void begin() {
    esp_task_wdt_init(TIMEOUT_S, true);
    esp_task_wdt_add(nullptr);
}

inline void feed() {
    esp_task_wdt_reset();
}

}
