/*
 * BLE central implementation using ESP-IDF's Bluedroid GATT client API.
 *
 * STATUS: written against documented ESP-IDF esp_gap_ble_api.h /
 * esp_gattc_api.h patterns, but NOT built or tested against real ESP-IDF
 * headers or real hardware (no ESP-IDF toolchain was available in the
 * environment this was written in). Expect to need minor API fixups
 * against whatever ESP-IDF version you build with - check the official
 * `gattc_multi_connect` and `ble_hid_host_demo` examples in the esp-idf
 * repo if function signatures don't match.
 *
 * Known simplification that WILL need iterating against your real
 * controller: this subscribes to every notify-capable characteristic under
 * the HID service (0x1812) rather than precisely picking the input Report
 * characteristic via its Report Reference descriptor (0x2908). If the
 * controller exposes more than one Report characteristic (e.g. separate
 * ones for buttons vs. battery), you'll get more than one notify stream -
 * log everything first and figure out which handle carries the 17-byte
 * gamepad report before assuming this is done.
 */

#include "ble_central.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "ble_central";

#define HOGP_SERVICE_UUID       0x1812
#define GATT_CHAR_REPORT_UUID   0x2a4d
#define GATT_CCCD_UUID          0x2902

static esp_gatt_if_t s_gattc_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0;
static esp_bd_addr_t s_server_bda;
static int s_ready = 0;
static uint16_t s_output_char_handle = 0;
static BleCentralInputReportCb s_input_cb = NULL;

static esp_ble_scan_params_t s_scan_params = {
    .scan_type          = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval      = 0x50,
    .scan_window        = 0x30,
    .scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE,
};

static void subscribeToNotifyCharacteristics(void)
{
    // Enumerate every characteristic in the HID service and subscribe to
    // any that support notify. See the "known simplification" note above -
    // this is deliberately broad rather than precise.
    uint16_t count = 0;
    esp_gattc_char_elem_t* chars = NULL;

    esp_gattc_get_attr_count(s_gattc_if, s_conn_id, ESP_GATT_DB_CHARACTERISTIC,
        0, 0xffff, 0, &count);
    if (count == 0) {
        ESP_LOGE(TAG, "no characteristics found under HID service");
        return;
    }

    chars = malloc(sizeof(esp_gattc_char_elem_t) * count);
    if (!chars) {
        return;
    }

    if (esp_gattc_get_all_char(s_gattc_if, s_conn_id, 0, 0xffff, chars, &count, 0) != ESP_GATT_OK) {
        free(chars);
        return;
    }

    for (uint16_t i = 0; i < count; i++) {
        if (chars[i].uuid.len != ESP_UUID_LEN_16) {
            continue;
        }

        if (chars[i].uuid.uuid.uuid16 == GATT_CHAR_REPORT_UUID &&
            (chars[i].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)) {
            ESP_LOGI(TAG, "subscribing to report characteristic handle 0x%04x", chars[i].char_handle);
            esp_ble_gattc_register_for_notify(s_gattc_if, s_server_bda, chars[i].char_handle);
        }

        // The first writable (non-notify) Report characteristic is our best
        // guess for the output/rumble report - verify against real hardware.
        if (chars[i].uuid.uuid.uuid16 == GATT_CHAR_REPORT_UUID &&
            (chars[i].properties & (ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR)) &&
            s_output_char_handle == 0) {
            s_output_char_handle = chars[i].char_handle;
            ESP_LOGI(TAG, "guessing output report characteristic handle 0x%04x", s_output_char_handle);
        }
    }

    free(chars);
}

static void writeCccdForNotify(uint16_t char_handle)
{
    uint16_t count = 1;
    esp_gattc_descr_elem_t descr;
    if (esp_gattc_get_descr_by_char_handle(s_gattc_if, s_conn_id, char_handle,
            (esp_bt_uuid_t){ .len = ESP_UUID_LEN_16, .uuid.uuid16 = GATT_CCCD_UUID },
            &descr, &count) != ESP_GATT_OK || count == 0) {
        return;
    }

    uint16_t notify_en = 0x0001;
    esp_ble_gattc_write_char_descr(s_gattc_if, s_conn_id, descr.handle,
        sizeof(notify_en), (uint8_t*) &notify_en, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
}

static void gattcEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param)
{
    switch (event) {
    case ESP_GATTC_REG_EVT:
        s_gattc_if = gattc_if;
        break;

    case ESP_GATTC_CONNECT_EVT:
        s_conn_id = param->connect.conn_id;
        memcpy(s_server_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id);
        break;

    case ESP_GATTC_CFG_MTU_EVT: {
        esp_bt_uuid_t hogp_uuid = { .len = ESP_UUID_LEN_16, .uuid.uuid16 = HOGP_SERVICE_UUID };
        esp_ble_gattc_search_service(gattc_if, s_conn_id, &hogp_uuid);
        break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT:
        subscribeToNotifyCharacteristics();
        break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        writeCccdForNotify(param->reg_for_notify.handle);
        break;

    case ESP_GATTC_WRITE_DESCR_EVT:
        s_ready = 1;
        ESP_LOGI(TAG, "notifications enabled, relay is live");
        break;

    case ESP_GATTC_NOTIFY_EVT:
        if (s_input_cb) {
            s_input_cb(param->notify.value, param->notify.value_len);
        }
        break;

    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGW(TAG, "controller disconnected, restarting scan");
        s_ready = 0;
        s_output_char_handle = 0;
        bleCentralStartConnect();
        break;

    default:
        break;
    }
}

static void gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(0); // scan indefinitely until we find/connect
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            // TODO: filter by name (e.g. "Xbox Wireless Controller") or a
            // known BD address instead of connecting to the first LE
            // peripheral seen - this is a placeholder until you've
            // confirmed the controller's advertised name on real hardware.
            ESP_LOGI(TAG, "found BLE device, attempting connect");
            esp_ble_gap_stop_scanning();
            esp_ble_gattc_open(s_gattc_if, param->scan_rst.bda, param->scan_rst.ble_addr_type, true);
        }
        break;

    default:
        break;
    }
}

void bleCentralInit(BleCentralInputReportCb on_input_report)
{
    s_input_cb = on_input_report;
    esp_ble_gap_register_callback(gapEventHandler);
    esp_ble_gattc_register_callback(gattcEventHandler);
    esp_ble_gattc_app_register(0);
}

void bleCentralStartConnect(void)
{
    esp_ble_gap_set_scan_params(&s_scan_params);
}

int bleCentralIsReady(void)
{
    return s_ready;
}

int bleCentralSendOutputReport(const uint8_t* data, uint16_t len)
{
    if (!s_ready || s_output_char_handle == 0) {
        return -1;
    }

    esp_err_t err = esp_ble_gattc_write_char(s_gattc_if, s_conn_id, s_output_char_handle,
        len, (uint8_t*) data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    return err == ESP_OK ? 0 : -1;
}
