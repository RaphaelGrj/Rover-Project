#include <unity.h>
#include "PowerMath.h"

using namespace PowerMath;

void setUp(void) {}
void tearDown(void) {}

// --- INA219 --------------------------------------------------------------

// 50 mohm shunt: 10 uV per LSB -> 0.2 mA per LSB. 1 A through the shunt
// is 50 mV = 5000 LSB.
void test_ina219_current_from_shunt_raw(void) {
    TEST_ASSERT_EQUAL_INT32(1000, ina219CurrentMa(5000, 50));
    TEST_ASSERT_EQUAL_INT32(0, ina219CurrentMa(0, 50));
}

// Charging current flows the other way through the shunt: the sign must
// survive, it is how telemetry tells charge from discharge.
void test_ina219_current_keeps_sign_when_charging(void) {
    TEST_ASSERT_EQUAL_INT32(-950, ina219CurrentMa(-4750, 50));
}

void test_ina219_zero_shunt_never_divides_by_zero(void) {
    TEST_ASSERT_EQUAL_INT32(0, ina219CurrentMa(1234, 0));
}

// 8.400 V = 2100 LSB of 4 mV, sitting in bits 15..3; the flag bits
// below must not leak into the value.
void test_ina219_bus_voltage_skips_status_bits(void) {
    uint16_t raw = (uint16_t)(2100 << 3) | 0x0003;  // CNVR and OVF both set
    TEST_ASSERT_EQUAL_UINT32(8400, ina219BusMv(raw));
}

// --- BQ25887 encodings, pinned against the documented reset values -----

void test_bq_ichg_reset_value_decodes_to_1500ma(void) {
    TEST_ASSERT_EQUAL_UINT16(1500, bqIchgMa(0x5D));  // full reset byte, upper bits ignored
    TEST_ASSERT_EQUAL_HEX8(0x1D, bqIchgCode(1500));
}

void test_bq_ichg_round_trips_and_clamps(void) {
    TEST_ASSERT_EQUAL_UINT16(1000, bqIchgMa(bqIchgCode(1000)));
    TEST_ASSERT_EQUAL_UINT16(100, bqIchgMa(bqIchgCode(0)));
    TEST_ASSERT_EQUAL_UINT16(2200, bqIchgMa(bqIchgCode(5000)));
}

void test_bq_iindpm_reset_value_decodes_to_3000ma(void) {
    TEST_ASSERT_EQUAL_UINT16(3000, bqIindpmMa(0x39));
    TEST_ASSERT_EQUAL_HEX8(0x19, bqIindpmCode(3000));
    TEST_ASSERT_EQUAL_UINT16(2000, bqIindpmMa(bqIindpmCode(2000)));
    TEST_ASSERT_EQUAL_UINT16(500, bqIindpmMa(bqIindpmCode(100)));
}

void test_bq_vcellreg_reset_value_decodes_to_4200mv(void) {
    TEST_ASSERT_EQUAL_UINT16(4200, bqVcellregMv(0xA0));
    TEST_ASSERT_EQUAL_HEX8(0xA0, bqVcellregCode(4200));
    // Never above the chip's own ceiling, whatever the caller asks.
    TEST_ASSERT_EQUAL_UINT16(4600, bqVcellregMv(bqVcellregCode(9999)));
}

void test_bq_part_number_matches_reset_value_only(void) {
    TEST_ASSERT_TRUE(bqIsBq25887(0x28));
    TEST_ASSERT_TRUE(bqIsBq25887(0x2B));   // other DEV_REV, same part
    TEST_ASSERT_FALSE(bqIsBq25887(0x6A));  // e.g. an IMU's WHO_AM_I value
    TEST_ASSERT_FALSE(bqIsBq25887(0x00));
}

void test_bq_adc16_is_msb_first(void) {
    TEST_ASSERT_EQUAL_UINT16(0x0FA0, bqAdc16(0x0F, 0xA0));
}

void test_bq_charge_state_names(void) {
    TEST_ASSERT_EQUAL_STRING("none", bqChargeStateName(0x00));
    TEST_ASSERT_EQUAL_STRING("fast", bqChargeStateName(0xF3));  // upper flag bits ignored
    TEST_ASSERT_EQUAL_STRING("done", bqChargeStateName(0x06));
    TEST_ASSERT_TRUE(bqIsCharging(0x03));
    TEST_ASSERT_FALSE(bqIsCharging(0x06));
    TEST_ASSERT_FALSE(bqIsCharging(0x00));
}

void test_bq_watchdog_and_charge_enable_bits_match_reset_values(void) {
    // CTRL1 reset 0x9D: WATCHDOG = 01 (40 s), i.e. bit 4 set.
    TEST_ASSERT_EQUAL_HEX8(0x10, 0x9D & BQ_CTRL1_WATCHDOG_MASK);
    // CTRL2 reset 0x7D: AUTO_INDET_EN and EN_CHG both set by default.
    TEST_ASSERT_TRUE((0x7D & BQ_CTRL2_AUTO_INDET_EN) != 0);
    TEST_ASSERT_TRUE((0x7D & BQ_CTRL2_EN_CHG) != 0);
}

// --- Cell plausibility ----------------------------------------------------

void test_cells_plausible_when_they_add_up_to_the_pack(void) {
    TEST_ASSERT_TRUE(cellsPlausible(3900, 3910, 7800, 150));
}

// If the ADC scale assumption were wrong (eg. 2 mV/LSB), each cell would
// read half its value and the sum would no longer match the INA219.
void test_cells_rejected_when_scale_is_wrong(void) {
    TEST_ASSERT_FALSE(cellsPlausible(1950, 1955, 7800, 150));
}

// A broken mid-point wire reads as one cell near 0 V and the other near
// the whole pack: impossible values, rejected even if the sum agrees.
void test_cells_rejected_when_balance_wire_is_broken(void) {
    TEST_ASSERT_FALSE(cellsPlausible(7800, 0, 7800, 150));
}

void test_cells_rejected_when_sum_disagrees_with_pack(void) {
    TEST_ASSERT_FALSE(cellsPlausible(3900, 3900, 8200, 150));
}

void test_min_max_cell(void) {
    TEST_ASSERT_EQUAL_UINT16(3700, minCellMv(3700, 3900));
    TEST_ASSERT_EQUAL_UINT16(3900, maxCellMv(3700, 3900));
}

// --- Percent estimate ------------------------------------------------------

void test_battery_percent_is_clamped_and_linear(void) {
    TEST_ASSERT_EQUAL_UINT8(0, batteryPercentFromMv(6000, 6400, 8300));
    TEST_ASSERT_EQUAL_UINT8(100, batteryPercentFromMv(8400, 6400, 8300));
    TEST_ASSERT_EQUAL_UINT8(50, batteryPercentFromMv(7350, 6400, 8300));
    TEST_ASSERT_EQUAL_UINT8(0, batteryPercentFromMv(7000, 8300, 6400));  // nonsense config
}

// --- Low-battery sleep decision ---------------------------------------------

static BatterySnapshot onBattery(uint16_t minCell, uint32_t pack) {
    BatterySnapshot s = {};
    s.chargerOk = true;
    s.cellsValid = true;
    s.minCellMv = minCell;
    s.packValid = true;
    s.packMv = pack;
    return s;
}

void test_sleeps_when_one_cell_is_below_the_limit(void) {
    TEST_ASSERT_TRUE(batteryTooLowToRun(onBattery(2990, 6100), 3000));
    TEST_ASSERT_FALSE(batteryTooLowToRun(onBattery(3010, 6100), 3000));
}

// External power wins: the robot runs from USB and the cells are being
// filled, whatever they read right now.
void test_never_sleeps_on_usb_or_while_charging(void) {
    BatterySnapshot s = onBattery(2900, 5900);
    s.usbPresent = true;
    TEST_ASSERT_FALSE(batteryTooLowToRun(s, 3000));
    s.usbPresent = false;
    s.charging = true;
    TEST_ASSERT_FALSE(batteryTooLowToRun(s, 3000));
}

// Untrusted cells: the pack voltage decides, split over two cells.
void test_falls_back_to_pack_voltage_when_cells_are_untrusted(void) {
    BatterySnapshot s = onBattery(0, 5900);
    s.cellsValid = false;
    TEST_ASSERT_TRUE(batteryTooLowToRun(s, 3000));
    s.packMv = 6200;
    TEST_ASSERT_FALSE(batteryTooLowToRun(s, 3000));
}

// No battery at all (USB-only bring-up with the charger absent) reads
// ~0 V: that must never put the board to sleep.
void test_never_sleeps_on_an_implausible_or_missing_reading(void) {
    BatterySnapshot s = {};
    s.packValid = true;
    s.packMv = 0;
    TEST_ASSERT_FALSE(batteryTooLowToRun(s, 3000));
    s.packValid = false;
    s.packMv = 5000;
    TEST_ASSERT_FALSE(batteryTooLowToRun(s, 3000));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ina219_current_from_shunt_raw);
    RUN_TEST(test_ina219_current_keeps_sign_when_charging);
    RUN_TEST(test_ina219_zero_shunt_never_divides_by_zero);
    RUN_TEST(test_ina219_bus_voltage_skips_status_bits);
    RUN_TEST(test_bq_ichg_reset_value_decodes_to_1500ma);
    RUN_TEST(test_bq_ichg_round_trips_and_clamps);
    RUN_TEST(test_bq_iindpm_reset_value_decodes_to_3000ma);
    RUN_TEST(test_bq_vcellreg_reset_value_decodes_to_4200mv);
    RUN_TEST(test_bq_part_number_matches_reset_value_only);
    RUN_TEST(test_bq_adc16_is_msb_first);
    RUN_TEST(test_bq_charge_state_names);
    RUN_TEST(test_bq_watchdog_and_charge_enable_bits_match_reset_values);
    RUN_TEST(test_cells_plausible_when_they_add_up_to_the_pack);
    RUN_TEST(test_cells_rejected_when_scale_is_wrong);
    RUN_TEST(test_cells_rejected_when_balance_wire_is_broken);
    RUN_TEST(test_cells_rejected_when_sum_disagrees_with_pack);
    RUN_TEST(test_min_max_cell);
    RUN_TEST(test_battery_percent_is_clamped_and_linear);
    RUN_TEST(test_sleeps_when_one_cell_is_below_the_limit);
    RUN_TEST(test_never_sleeps_on_usb_or_while_charging);
    RUN_TEST(test_falls_back_to_pack_voltage_when_cells_are_untrusted);
    RUN_TEST(test_never_sleeps_on_an_implausible_or_missing_reading);
    return UNITY_END();
}
