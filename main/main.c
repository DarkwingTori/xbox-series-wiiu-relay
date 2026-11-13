/*
 * Xbox Series X/S -> Wii U Bluetooth relay.
 *
 * Flow: BLE-central connects to the Xbox Series controller and receives its
 * HOGP input reports -> translate() reshapes those bytes into the report
 * layout Bloopair's xbox_one_controller.c already expects -> Classic-BT
 * peripheral sends that as an HID input report to the Wii U, which sees an
 * "Xbox One S controller". Rumble output reports flow the reverse way.
 */

#include "ble_central.h"
#include "hid_peripheral.h"
#include "relay_report.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "relay";

// Called from ble_central's GATTC notify handler with a raw HOGP input
// report from the Xbox Series controller.
static void onControllerInputReport(const uint8_t* data, uint16_t len)
{
    if (len == 0) {
        return;
    }

    // The Series controller's BLE HOGP input report uses the same report ID
    // and byte layout as XboxRelayInputReport (see relay_report.h), so it is
    // forwarded as-is.
    if (data[0] == XBOX_INPUT_REPORT_ID) {
        hidPeripheralSendInputReport(XBOX_INPUT_REPORT_ID, data, len);
    } else {
        ESP_LOGW(TAG, "unhandled report id 0x%02x len %u from controller", data[0], len);
    }
}

// Called from hid_peripheral's ESP_HIDD_OUTPUT_EVT with an outgoing
// (rumble) report the Wii U/Bloopair sent.
static void onWiiUOutputReport(const uint8_t* data, uint16_t len)
{
    if (len == 0 || data[0] != XBOX_OUTPUT_REPORT_ID) {
        return;
    }

    bleCentralSendOutputReport(data, len);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Bring up the BLE side first and wait for it to be ready before
    // starting the Classic BT peripheral side, since scan/connect/advertise
    // operations contend for the single radio (see project README).
    bleCentralInit(onControllerInputReport);
    bleCentralStartConnect();

    ESP_LOGI(TAG, "put the Xbox Series controller into pairing mode now");
    for (int waited_s = 0; !bleCentralIsReady(); waited_s++) {
        if (waited_s > 0 && waited_s % 20 == 0) {
            ESP_LOGW(TAG, "still waiting for the Xbox controller (%ds) - hold its pair button", waited_s);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "controller connected, starting Wii U side");
    hidPeripheralInit(onWiiUOutputReport);
    hidPeripheralStartAdvertising();

    ESP_LOGI(TAG, "pair this device from the Wii U's Bloopair controller list now");
}
