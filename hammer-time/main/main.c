// vim: foldmethod=marker:foldmarker={{{,}}}
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdlib.h>

#include "server.h"
#include "wifi.h"

static const char *TAG = "ht-main";

void setup_chip() // {{{
{
    /* load chip info, flash size, etc. */
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    esp_chip_info(&chip_info);
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size.");
        return;
    }

    /* print chip info */
    ESP_LOGI(
        TAG, "%s, %d core, WiFi%s%s%s, v%d.%d, %" PRIu32 "MB %s flash, %" PRIu32 "KB min free heap",
        CONFIG_IDF_TARGET, chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
        (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
        (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "",
        major_rev, minor_rev, flash_size / (uint32_t)(1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external",
        esp_get_minimum_free_heap_size() / (uint32_t)1024);

    /* initialize non-volatile storage, erasing if old */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
} // }}}

void app_main(void) // {{{
{
    ESP_LOGI(TAG, "Hammer Controller v1");
    setup_chip();
    wifi_init();
    server_init();
} // }}}
