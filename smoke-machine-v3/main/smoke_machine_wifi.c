// vim: foldmethod=marker:foldmarker={{{,}}}
#include "smoke_machine_wifi.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "secrets.h"

static const char *TAG = "sm-wifi";

/* wifi/ip event handler {{{ */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                          void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* wifi connection is possible */
        esp_wifi_connect();
        ESP_LOGI(TAG, "attempting connection");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* wifi connection lost, retry */
        esp_wifi_connect();
        ESP_LOGI(TAG, "connection failed, retrying");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* recieved ip, implies connection success */
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        uint8_t mac[6] = {
            0,
        };
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        ESP_LOGI(TAG, "connected, mac: " MACSTR ", ip: " IPSTR, MAC2STR(mac),
                 IP2STR(&event->ip_info.ip));
    }
}
/* wifi/ip event handler }}} */

void smoke_machine_wifi_init(void)
{
    /* initialize network interface */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* init wifi stack with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* register our event_handler function to capture all WIFI and some IP events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* update our wifi config with provided SSID and PASS
     *   having a password implies all security modes including WEP/WPA so we
     *   force WPA2 by setting .threshold.authmode
     */
    wifi_config_t wifi_config = {
        .sta = {.ssid = WIFI_SSID,
                .password = WIFI_PASS,
                .threshold = {.authmode = WIFI_AUTH_WPA2_PSK}},
    };

    /* switch WIFI to station mode, provide our config, and start subsystem */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
