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

## Status

**Confirmed working on real hardware** - built, flashed to an ESP32, and
tested end-to-end with a real Xbox Series controller and a real Wii U
running Bloopair. The controller pairs and works through the relay as
designed.

The items below were the open implementation risks called out before this
was tested. They weren't each individually re-verified one by one after the
fact - only that the relay works overall - so treat them as implementation
notes on how this was built rather than a checklist of confirmed-correct
specifics if you're adapting this code for a different controller/console
pairing:

1. **VID/PID exposure.** Bloopair identifies controllers by reading a
   Bluetooth SDP Device ID (DI) record after pairing. `main/hid_peripheral.c`
   registers the HID device app expecting this to expose `045e:02fd` to the
   Wii U - see the comment at the top of that file for the reasoning.
2. **Report byte layout.** `relay_report.h` assumes the Series controller's
   BLE HOGP input report is byte-identical to the Xbox One S's Classic
   report (same field order/widths), based on public documentation
   (xpadneo).
3. **GATT characteristic selection.** `ble_central.c` subscribes to every
   notify-capable Report characteristic (`0x2A4D`) under the HID service
   rather than precisely identifying the gamepad one via its Report
   Reference descriptor (`0x2908`).
4. **SSP / pairing mode.** `sdkconfig.defaults` disables Secure Simple
   Pairing based on a note from Bloopair's own research (its Bluetooth
   stack expects SSP disabled on emulated/third-party controllers).
5. **Simultaneous Classic + BLE radio use.** `main.c` finishes BLE pairing
   with the controller before starting Classic BT advertising toward the
   Wii U, since scan/connect/advertise operations contend for the single
   radio.

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
