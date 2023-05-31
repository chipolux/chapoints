#include "smoke_machine_wifi.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include "secrets.h"

#define WIFI_RETRY 5

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "sm-wifi";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* retry count */
static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                          void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* wifi connection becomes possible */
        esp_wifi_connect();
        ESP_LOGD(TAG, "initial connection attempt");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* wifi connection lost */
        if (s_retry_num < WIFI_RETRY) {
            /* attempt connection again and use up a retry */
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retrying connection, %d left", WIFI_RETRY - s_retry_num);
        } else {
            /* exhausted retries, signaling failure */
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGW(TAG, "connection failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* recieved ip, implies connection success */
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "connected, ip: %s", ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void smoke_machine_wifi_init(void)
{
    /* create event group for signalling connected/failed */
    s_wifi_event_group = xEventGroupCreate();

    /* setup tcpip stack */
    tcpip_adapter_init();

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

    /* wait until we recieve either of our bits, they are set in the event_handler()  */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    /* check the bits that were returned to see if we failed or succeeded */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected, ssid: %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "failed to connect, ssid: %s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "failed with unknown event: %d", bits);
    }

    // NOTE: by leaving the event handler working we will automatically attempt
    //       to reconnect if we lose connection. this seems to be fine...
    // /* unregister our event handler since we are done with the connection attempt */
    // ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    // ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));

    // /* delete the event group since we will no longer be recieving any bits */
    // vEventGroupDelete(s_wifi_event_group);
}
