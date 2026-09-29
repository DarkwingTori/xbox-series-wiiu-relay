/*
 * Classic Bluetooth HID device (peripheral) role: presents this ESP32 to
 * the Wii U as an Xbox One S controller (VID 0x045e PID 0x02fd), which
 * Bloopair's existing, unmodified xbox_one_controller.c driver already
 * understands.
 */
#pragma once

#include <stdint.h>

// Called when the Wii U/Bloopair sends an outgoing (rumble) HID report.
typedef void (*HidPeripheralOutputReportCb)(const uint8_t* data, uint16_t len);

void hidPeripheralInit(HidPeripheralOutputReportCb on_output_report);

// Starts advertising/making the ESP32 connectable as a Classic BT HID
// device so it shows up in the Wii U's Bloopair controller-pairing screen.
void hidPeripheralStartAdvertising(void);

int hidPeripheralIsConnected(void);

// Sends an input report (e.g. XBOX_INPUT_REPORT_ID) to the connected host.
int hidPeripheralSendInputReport(uint8_t report_id, const uint8_t* data, uint16_t len);
