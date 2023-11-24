// vim: foldmethod=marker:foldmarker={{{,}}}
#include "wifi.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "secrets.h"

static const char *TAG = "ht-wifi";

/* wifi/ip sta event handler {{{ */
static void sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
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
    } else {
        ESP_LOGI(TAG, "unknown %s event id: %ld", event_base == WIFI_EVENT ? "wifi" : "ip",
                 event_id);
    }
}
/* wifi/ip sta event handler }}} */

/* wifi sta init {{{ */
void init_sta()
{
    /* initialize network interface */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta(); /* returns esp_netif_t *sta */

    /* init wifi stack with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* register our event_handler function to capture all WIFI and some IP events */
    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &sta_event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &sta_event_handler, NULL));

    /* update our wifi config with provided SSID and PASS
     *   having a password implies all security modes including WEP/WPA so we
     *   force WPA2 by setting .threshold.authmode
     */
    wifi_config_t wifi_config = {
        .sta = {.ssid = /* AP_SSID */ WIFI_SSID,
                .password = /* AP_PASS */ WIFI_PASS,
                .threshold = {.authmode = WIFI_AUTH_WPA2_PSK}},
    };

    /* switch WIFI to station mode, provide our config, and start subsystem */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
/* wifi sta init }}} */

void wifi_init(void)
{
    // esp_log_level_set("wifi", ESP_LOG_DEBUG);

    init_sta();
}
