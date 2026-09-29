/*
 * Classic Bluetooth HID device implementation using ESP-IDF's
 * esp_hidd_api.h (Bluedroid Classic BT HID Device profile), following the
 * pattern of the esp-idf `bt_hid_mouse_device` example.
 *
 * STATUS: written against documented ESP-IDF esp_hidd_api.h patterns, NOT
 * built or tested (no ESP-IDF toolchain available in the environment this
 * was written in).
 *
 * KNOWN GAP that needs real investigation: getting the Wii U/Bloopair to
 * see this device's VID/PID as 0x045e/0x02fd. Bloopair reads vendor_id and
 * product_id from the paired device's SDP Device ID (DI) record (see
 * Bloopair's ios/ios_pad/source/bta/bta_hh_sdp.c - it hooks SDP_DiDiscover
 * specifically to read this). esp_hidd_api's esp_hidd_app_param_t (as used
 * in the mouse example) does not obviously expose VID/PID fields for a DI
 * record in older ESP-IDF versions - you may need to register a separate
 * SDP Device ID Profile record yourself via the lower-level esp_sdp_api.h
 * (SDP_CreateRecord / SDP DI attributes: SpecificationID, VendorID,
 * ProductID, Version) alongside the HID SDP record esp_hidd_api creates
 * automatically. Check your ESP-IDF version's esp_hidd_api.h and
 * esp_sdp_api.h headers directly - this is the single biggest unknown left
 * in this project and needs to be nailed down against real hardware before
 * anything else here matters.
 */

#include "hid_peripheral.h"
#include "hid_descriptor.h"
#include "esp_hidd_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "hid_peripheral";

static int s_connected = 0;
static HidPeripheralOutputReportCb s_output_cb = NULL;

static void hiddCallback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t* param)
{
    switch (event) {
    case ESP_HIDD_INIT_EVT:
        ESP_LOGI(TAG, "hidd init, registering app");
        break;

    case ESP_HIDD_CONNECT_EVT:
        s_connected = 1;
        ESP_LOGI(TAG, "Wii U connected");
        break;

    case ESP_HIDD_DISCONNECT_EVT:
        s_connected = 0;
        ESP_LOGW(TAG, "Wii U disconnected, re-advertising");
        hidPeripheralStartAdvertising();
        break;

    case ESP_HIDD_OUTPUT_EVT:
        if (s_output_cb) {
            s_output_cb(param->output.data, param->output.len);
        }
        break;

    default:
        break;
    }
}

void hidPeripheralInit(HidPeripheralOutputReportCb on_output_report)
{
    s_output_cb = on_output_report;

    // Advertised name/VID-PID should match a real Xbox One S controller as
    // closely as this API allows - see the file-level comment above about
    // the VID/PID SDP DI record gap.
    esp_bt_dev_set_device_name("Xbox Wireless Controller");

    // Set device class to Peripheral / Gamepad so the Wii U's controller
    // scan lists it sensibly.
    esp_bt_cod_t cod = {
        .major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL,
        .minor = 0x08, // Gamepad, per Bluetooth Assigned Numbers peripheral minor device class
    };
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);

    esp_hidd_register_callback(hiddCallback);
    esp_hidd_init();

    esp_hidd_app_param_t app_param = {
        .name = "Xbox Wireless Controller",
        .description = "Xbox Series X|S -> Wii U relay",
        .provider = "Microsoft",
        .subclass = ESP_HID_CLASS_GPD, // Gamepad
        .desc_list = (uint8_t*) xbox_relay_hid_descriptor,
        .desc_list_len = XBOX_RELAY_HID_DESCRIPTOR_LEN,
    };

    esp_hidd_qos_param_t qos = { 0 };
    esp_bt_hid_device_register_app(&app_param, &qos, &qos);
}

void hidPeripheralStartAdvertising(void)
{
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}

int hidPeripheralIsConnected(void)
{
    return s_connected;
}

int hidPeripheralSendInputReport(uint8_t report_id, const uint8_t* data, uint16_t len)
{
    if (!s_connected) {
        return -1;
    }

    esp_hidd_send_report(ESP_HIDD_REPORT_TYPE_INTRDATA, report_id, len, (uint8_t*) data);
    return 0;
}
