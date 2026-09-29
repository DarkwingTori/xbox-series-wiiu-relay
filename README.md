# Xbox Series X/S -> Wii U Bluetooth relay

An ESP32 firmware project that lets an Xbox Series X/S controller work on a
Wii U through [Bloopair](https://github.com/GaryOderNichts/Bloopair), without
any changes to Bloopair or the console itself.

## How it works

Xbox Series X/S controllers only pair over Bluetooth LE (HID over GATT).
Bloopair's Bluetooth stack only understands Bluetooth Classic HID - it works
by patching Nintendo's already-running, closed Bluetooth firmware, which
doesn't have the LE/GATT layers needed. Reverse-engineering that closed
firmware to add real BLE support was investigated and ruled impractical for
now (see the Bloopair-side research this project came out of - no one,
including Bloopair's own maintainer, has done it in 4+ years).

Instead, this ESP32 does both halves of the protocol conversion itself:

- **BLE central** to the Xbox Series controller (`ble_central.c`) - standard,
  well-documented Bluetooth LE, no reverse engineering needed.
- **Classic Bluetooth HID device (peripheral)** to the Wii U (`hid_peripheral.c`)
  - presents itself as an **Xbox One S controller** (VID `0x045e` PID
    `0x02fd`), an identity Bloopair's existing, unmodified
    `xbox_one_controller.c` driver already fully supports.

The Wii U pairs with the ESP32 through Bloopair's normal controller-pairing
flow, believing it's a real Xbox One S controller. The ESP32 relays live
input from your actual Series controller underneath.

## Status - read before wiring anything up

**None of this has been built, flashed, or tested.** There was no ESP-IDF
toolchain available in the environment this was written in, so everything
here is written carefully against documented ESP-IDF APIs and examples, not
verified against a real compiler or real hardware. Expect to spend real time
debugging this on your bench. Known specific gaps, in rough order of how
much they'll bite you:

1. **VID/PID exposure.** Bloopair identifies controllers by reading a
   Bluetooth SDP Device ID (DI) record after pairing. It's not confirmed
   that ESP-IDF's `esp_hidd_api` HID Device profile registers a DI record
   with a custom VID/PID out of the box - see the big comment at the top of
   `main/hid_peripheral.c`. If the Wii U doesn't see `045e:02fd`, Bloopair
   won't route it to the Xbox One driver at all, and nothing else here will
   matter until this is fixed (likely needs a hand-rolled SDP DI record via
   `esp_sdp_api.h` alongside the HID one).
2. **Report byte layout assumption.** `relay_report.h` assumes the Series
   controller's BLE HOGP input report is byte-identical to the Xbox One S's
   Classic report (same field order/widths). That's based on public
   documentation (xpadneo), not a capture of your specific controller. First
   thing to do once BLE-central is connecting: log the raw notification
   bytes and compare against `XboxRelayInputReport` before trusting the
   passthrough in `main.c`.
3. **Picking the right GATT characteristic.** `ble_central.c` currently
   subscribes to every notify-capable Report characteristic (`0x2A4D`) under
   the HID service rather than precisely identifying the gamepad one via its
   Report Reference descriptor (`0x2908`). If the controller exposes more
   than one, you'll need to disambiguate by handle/length once you can see
   real traffic.
4. **SSP / pairing mode.** `sdkconfig.defaults` disables Secure Simple
   Pairing based on a note from Bloopair's own research (its Bluetooth
   stack expects SSP disabled on emulated/third-party controllers) - this
   should be right but hasn't been confirmed against this specific flow.
5. **Simultaneous Classic + BLE radio use.** `main.c` deliberately finishes
   BLE pairing with the controller before starting Classic BT advertising
   toward the Wii U, since scan/connect/advertise operations contend for the
   single radio. Steady-state relay of both links at once should be fine
   (this is the normal combo-chip case), but if it isn't stable in practice,
   fall back to two ESP32 boards - one per role, linked over UART - which is
   a strictly simpler, more proven architecture at the cost of a second
   board.

## Hardware

An **original ESP32** (not S2/C3, which lack the Classic Bluetooth radio).
Any ESP32 dev board with Classic BT + BLE works.

## Building

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)
installed and sourced (`. $IDF_PATH/export.sh`).

```
idf.py set-target esp32
idf.py menuconfig   # double-check the Bluetooth options in sdkconfig.defaults actually applied
idf.py build
idf.py -p <PORT> flash monitor
```

## Files

- `main/main.c` - top-level flow, wires BLE central input -> HID peripheral output and back
- `main/ble_central.{h,c}` - BLE GATT client, connects to the Xbox Series controller
- `main/hid_peripheral.{h,c}` - Classic BT HID device, impersonates the Xbox One S to the Wii U
- `main/relay_report.h` - the shared wire-format struct (mirrors Bloopair's `XboxOneInputReport`)
- `main/hid_descriptor.h` - the Classic BT HID report descriptor advertised to the Wii U
