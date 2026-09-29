/*
 * Classic Bluetooth HID report descriptor advertised to the Wii U.
 *
 * IMPORTANT: Bloopair does NOT parse this descriptor at runtime - its
 * xbox_one_controller.c driver hardcodes the known byte offsets for report
 * ID 0x01 (see relay_report.h / XboxRelayInputReport). This descriptor only
 * needs to be well-formed enough for the Wii U's standard Bluetooth HID
 * profile/SDP handshake to accept the connection as a valid HID gamepad -
 * it does not need to byte-for-byte match Microsoft's real descriptor.
 *
 * That said, this describes report ID 0x01 with the same field widths/order
 * as XboxRelayInputReport (4x 16-bit stick axes, 2x 16-bit analog triggers,
 * a 4-bit hat switch, and button bits), and report ID 0x03 as an 8-byte
 * vendor output report for rumble, matching XboxRelayOutputReport.
 */
#pragma once

#include <stdint.h>

static const uint8_t xbox_relay_hid_descriptor[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x05,       // Usage (Game Pad)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       //   Report ID (1)

    // 4x 16-bit stick axes: X, Y, Rx, Ry
    0x05, 0x01,       //   Usage Page (Generic Desktop)
    0x09, 0x30,       //   Usage (X)
    0x09, 0x31,       //   Usage (Y)
    0x09, 0x33,       //   Usage (Rx)
    0x09, 0x34,       //   Usage (Ry)
    0x16, 0x00, 0x80, //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F, //   Logical Maximum (32767)
    0x75, 0x10,       //   Report Size (16)
    0x95, 0x04,       //   Report Count (4)
    0x81, 0x02,       //   Input (Data,Var,Abs)

    // 2x 16-bit analog triggers: Z, Rz
    0x09, 0x32,       //   Usage (Z)
    0x09, 0x35,       //   Usage (Rz)
    0x16, 0x00, 0x00, //   Logical Minimum (0)
    0x26, 0xFF, 0xFF, //   Logical Maximum (65535)
    0x75, 0x10,       //   Report Size (16)
    0x95, 0x02,       //   Report Count (2)
    0x81, 0x02,       //   Input (Data,Var,Abs)

    // 4-bit hat switch (dpad)
    0x09, 0x39,       //   Usage (Hat Switch)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x07,       //   Logical Maximum (7)
    0x35, 0x00,       //   Physical Minimum (0)
    0x46, 0x3B, 0x01, //   Physical Maximum (315)
    0x65, 0x14,       //   Unit (Eng Rot:Angular Pos)
    0x75, 0x04,       //   Report Size (4)
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x42,       //   Input (Data,Var,Abs,Null)

    // 4-bit padding to byte-align after the hat switch
    0x75, 0x04,       //   Report Size (4)
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x03,       //   Input (Const,Var,Abs)

    // 16 buttons
    0x05, 0x09,       //   Usage Page (Button)
    0x19, 0x01,       //   Usage Minimum (Button 1)
    0x29, 0x10,       //   Usage Maximum (Button 16)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x10,       //   Report Count (16)
    0x81, 0x02,       //   Input (Data,Var,Abs)

    // Output report ID 3: 8-byte vendor-defined rumble payload
    0x85, 0x03,       //   Report ID (3)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined)
    0x09, 0x01,       //   Usage (Vendor Usage 1)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08,       //   Report Size (8)
    0x95, 0x08,       //   Report Count (8)
    0x91, 0x02,       //   Output (Data,Var,Abs)

    0xC0,             // End Collection
};

#define XBOX_RELAY_HID_DESCRIPTOR_LEN sizeof(xbox_relay_hid_descriptor)
