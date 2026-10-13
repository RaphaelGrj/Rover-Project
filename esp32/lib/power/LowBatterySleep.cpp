#include "LowBatterySleep.h"
#include <esp_sleep.h>
#include <esp_attr.h>
#include "PowerMath.h"
#include "BoardIo.h"
#include "Watchdog.h"

namespace {

// Survives deep sleep (RTC memory), not a power cycle: a fresh battery
// plugged in starts from the normal threshold again.
RTC_DATA_ATTR bool sleptForLowBattery = false;

PowerMath::BatterySnapshot snapshot(const BatteryMonitor& battery, const ChargerMonitor& charger) {
    PowerMath::BatterySnapshot s = {};
    s.chargerOk = charger.ok();
    s.usbPresent = charger.usbPresent();
    s.charging = charger.charging();
    s.cellsValid = charger.cellsValid();
    s.minCellMv = PowerMath::minCellMv(charger.cellTopMv(), charger.cellBottomMv());
    s.packValid = battery.hasReading();
    s.packMv = battery.packMv();
    return s;
}

}  // namespace

namespace LowBatterySleep {

bool wokeFromLowBatterySleep() {
    return sleptForLowBattery;
}

bool tooLowAtBoot(BatteryMonitor& battery, ChargerMonitor& charger) {
    if (!ROVER_BATTERY_MONITORING_ENABLED || !ROVER_BATTERY_USES_INA219) return false;

    // Neither chip has a value yet: the INA219 needs ~136 ms for its
    // first averaged shunt + bus pair, the BQ25887 ~200 ms for a full
    // ADC cycle. Reading earlier would give 0 V -- which the decision
    // rule ignores, but which would also let a truly flat robot boot.
    delay(250);
    RoverWatchdog::feed();
    battery.sampleNow();
    charger.sampleNow(battery.packMv(), battery.hasReading());

    uint16_t limit = sleptForLowBattery ? ROVER_CELL_RESUME_MV : ROVER_CELL_SHUTDOWN_MV;
    bool tooLow = PowerMath::batteryTooLowToRun(snapshot(battery, charger), limit);
    if (!tooLow) sleptForLowBattery = false;
    return tooLow;
}

bool tooLowNow(const BatteryMonitor& battery, const ChargerMonitor& charger) {
    if (!ROVER_BATTERY_MONITORING_ENABLED || !ROVER_BATTERY_USES_INA219) return false;
    return PowerMath::batteryTooLowToRun(snapshot(battery, charger), ROVER_CELL_SHUTDOWN_MV);
}

void enterSleep() {
    // Expander lines keep their level through deep sleep (the chip stays
    // powered), so this is what actually cuts the amps, backlight,
    // camera and right ToF. Motor inputs float in deep sleep and are
    // held low by the DRV8833's own pull-downs: no motion possible.
    BoardIo::prepareForSleep();
    Serial.flush();
    sleptForLowBattery = true;
    esp_sleep_enable_timer_wakeup((uint64_t)ROVER_LOW_BATTERY_WAKE_PERIOD_S * 1000000ULL);
    esp_deep_sleep_start();
}

}  // namespace LowBatterySleep
