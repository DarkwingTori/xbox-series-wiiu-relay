/*
 * Classic Bluetooth HID device implementation using ESP-IDF's
 * esp_hidd_api.h (Bluedroid Classic BT HID Device profile), following the
 * pattern of the esp-idf `bt_hid_mouse_device` example.
 *
 * Bloopair reads vendor_id and product_id (0x045e/0x02fd) from the paired
 * device's SDP Device ID (DI) record (see Bloopair's
 * ios/ios_pad/source/bta/bta_hh_sdp.c, which hooks SDP_DiDiscover). The HID
 * SDP record created by esp_hidd_api is what the Wii U sees here; the
 * advertised name and device class below are set to match an Xbox controller.
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

    // Advertised name should match a real Xbox One S controller.
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
