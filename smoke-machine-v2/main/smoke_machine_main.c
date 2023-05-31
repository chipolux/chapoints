#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "smoke_machine_server.h"
#include "smoke_machine_wifi.h"

/* tag for log messages */
static const char *TAG = "sm-main";

void app_main()
{
    /* startup and chip info */
    ESP_LOGI(TAG, "Smoke Machine Controller v2");
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "ESP8266, %d cores, rev %d, %dMB %s flash", chip_info.cores, chip_info.revision,
             spi_flash_get_chip_size() / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    /* initialize non-volatile storage */
    ESP_ERROR_CHECK(nvs_flash_init());
    /* initialize network interface stack */
    ESP_ERROR_CHECK(esp_netif_init());
    /* setup default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* setup wifi connection */
    smoke_machine_wifi_init();
    /* setup webserver */
    smoke_machine_server_init();

    ESP_LOGI(TAG, "end of app_main() reached");
}
