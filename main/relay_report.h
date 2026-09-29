/*
 * Shared report layout between the BLE-central side (Xbox Series controller)
 * and the Classic-BT-peripheral side (Wii U / Bloopair).
 *
 * This mirrors XboxOneInputReport from Bloopair's
 * ios/ios_pad/source/controllers/xbox_one_controller.h byte-for-byte, since
 * that's the exact layout Bloopair's existing (unmodified) driver expects
 * for VID 0x045e PID 0x02fd. Today's Bloopair-side research found the
 * Series controller uses the same application-layer report layout over
 * BLE HOGP - but that was based on public documentation (xpadneo), not a
 * byte capture of your specific controller/firmware. Before trusting this,
 * log the raw bytes from ble_central's notification callback and diff them
 * against this struct - if fields are shifted, fix this struct/the
 * translation in main.c, not the Wii U side.
 *
 * Bit order within each byte below is written in the exact same declaration
 * order as Bloopair's original (first declared field = LSB, GCC's default
 * for single-byte bitfield groups on both the ARM target Bloopair runs on
 * and the Xtensa/RISC-V target this runs on - bitfield allocation order for
 * a single-byte storage unit isn't affected by overall multi-byte
 * endianness). Don't reorder these declarations without re-checking against
 * xbox_one_controller.h in the Bloopair checkout.
 */
#pragma once

#include <stdint.h>

#define XBOX_INPUT_REPORT_ID    0x01
#define XBOX_OUTPUT_REPORT_ID   0x03

#define XBOX_INPUT_REPORT_LEN  17
#define XBOX_OUTPUT_REPORT_LEN 9

typedef struct __attribute__((packed)) {
    uint8_t report_id; // XBOX_INPUT_REPORT_ID

    uint16_t left_stick_x;
    uint16_t left_stick_y;
    uint16_t right_stick_x;
    uint16_t right_stick_y;

    uint16_t left_trigger;
    uint16_t right_trigger;

    uint8_t : 4;
    uint8_t dpad : 4;

    uint8_t rb : 1;
    uint8_t lb : 1;
    uint8_t : 1;
    uint8_t y : 1;
    uint8_t x : 1;
    uint8_t : 1;
    uint8_t b : 1;
    uint8_t a : 1;

    uint8_t : 1;
    uint8_t rstick : 1;
    uint8_t lstick : 1;
    uint8_t xbox : 1;
    uint8_t menu : 1;
    uint8_t : 3;

    uint8_t : 7;
    uint8_t view : 1;
} XboxRelayInputReport;

typedef struct __attribute__((packed)) {
    uint8_t report_id; // XBOX_OUTPUT_REPORT_ID
    uint8_t motors_enable;
    uint8_t magnitude_left;
    uint8_t magnitude_right;
    uint8_t magnitude_strong;
    uint8_t magnitude_weak;
    uint8_t pulse_sustain_10ms;
    uint8_t pulse_release_10ms;
    uint8_t loop_count;
} XboxRelayOutputReport;
