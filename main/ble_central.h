/*
 * BLE central role: connects to the Xbox Series X/S controller over
 * Bluetooth LE, discovers its HID over GATT (HOGP) service, and delivers
 * raw input report notifications via a callback. Also exposes a function to
 * write outgoing (rumble) reports back to the controller.
 */
#pragma once

#include <stdint.h>

// Called whenever a HOGP input report notification arrives from the
// controller. `data`/`len` is the raw report payload (report ID byte
// included, same shape as XboxRelayInputReport in relay_report.h - but
// verify this on real hardware before trusting it, see relay_report.h).
typedef void (*BleCentralInputReportCb)(const uint8_t* data, uint16_t len);

void bleCentralInit(BleCentralInputReportCb on_input_report);

// Starts scanning for and connecting to the controller. Put the controller
// into BLE pairing mode (hold the pair button until the Xbox button flashes
// fast) before calling this.
void bleCentralStartConnect(void);

// Returns 1 once connected, service-discovered, and subscribed to input
// report notifications.
int bleCentralIsReady(void);

// Writes an outgoing report (e.g. rumble, XBOX_OUTPUT_REPORT_ID) to the
// controller's HOGP output report characteristic. Returns 0 on success.
int bleCentralSendOutputReport(const uint8_t* data, uint16_t len);
