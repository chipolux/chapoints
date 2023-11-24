// vim: foldmethod=marker:foldmarker={{{,}}}
#include "server.h"

#include <stdlib.h>
#include <sys/param.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/task.h"

#include "stepper.h"

static const char *TAG = "ht-server";

static httpd_handle_t server = NULL;

static const uint16_t steps_per_rot = 3511;
// 2048 steps per rotation fed into 28t -> 48t gear ratio = 3510.857...

#define CLAMP(value, min, max) (value % (max + 1 - min) + min)

void queue_smacks(const uint8_t count) // {{{
{
    ESP_LOGI(TAG, "setting up %i smacks...", count);
    // NOTE: expect hammer to rest roughly perpendicular to smack target so a
    //       quarter rotation will hit!
    stepper_enqueue(steps_per_rot * 0.25, 1, false); // initial smack
    // smacks (after the initial one) should be random!
    for (int i = 1; i < count; ++i) {
        uint32_t rand = esp_random();
        // we want random amounts of steps (up to 0.15 * full rotation)
        float steps = steps_per_rot * ((float)CLAMP(rand, 8, 15) / 100.0f);
        stepper_enqueue(steps, -1, false); // pull back
        // around 25% of the time we want to wait a random delay
        if (rand % 100 < 25) {
            stepper_enqueue((uint16_t)CLAMP(rand, 50, 500), 0, false); // delay
        }
        stepper_enqueue(steps, 1, false); // smack again
    }
    stepper_enqueue(steps_per_rot * 0.25, -1, true); // reset back and put motors to sleep
} // }}}

/* root handler {{{ */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char resp_str[] =
        "<html>"
        "  <head>"
        "    <meta charset=\"UTF-8\">"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "    <style>"
        "      html, body { margin: 10px; padding: 0; }"
        "      body { display: flex; flex-direction: column; }"
        "      button, input { margin: 5px; padding: 5px; background: orange; font-size: 2em; }"
        "    </style>"
        "    <script type=\"text/javascript\">"
        "      function activate() {"
        "        var count = document.getElementById(\"count\").value;"
        "        fetch(`/activate?count=${count}`, {method: \"POST\"});"
        "      }"
        "    </script>"
        "  </head>"
        "  <button onclick=\"activate()\">Activate</button>"
        "  <input type=\"number\" id=\"count\" min=\"1\" max=\"5\" value=\"3\">"
        "</html>";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}
static httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
/* root handler }}} */

/* activate handler {{{ */
static esp_err_t activate_post_handler(httpd_req_t *req)
{
    /* read url query string length and alloc memory for it (+1 for null) */
    char *buf;
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "activate with query: %s", buf);
            char param[32];
            if (httpd_query_key_value(buf, "count", param, sizeof(param)) == ESP_OK) {
                uint8_t count = (uint8_t)atoi(param);
                if (count < 1 || count > 5) {
                    /* default to 3 smacks */
                    count = 3;
                }
                ESP_LOGI(TAG, "parsed count: %d", count);
                queue_smacks(count);
            }
        }
        free(buf);
    }

    const char resp_str[] = "activated";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}
static httpd_uri_t activate = {
    .uri = "/activate", .method = HTTP_POST, .handler = activate_post_handler};
/* activate handler }}} */

/* start webserver {{{ */
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "starting on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "registering uri handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &activate);
        return server;
    }

    ESP_LOGW(TAG, "error starting server!");
    return NULL;
}
/* start webserver }}} */

static esp_err_t stop_webserver(httpd_handle_t server) { return httpd_stop(server); }

/* wifi disconnect handler {{{ */
static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server) {
        ESP_LOGI(TAG, "stopping webserver, wifi disconnected");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "failed to stop webserver");
        }
    }
}
/* wifi disconnect handler }}} */

/* wifi connect handler {{{ */
static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                            void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "starting webserver, wifi connected");
        *server = start_webserver();
    }
}
/* wifi connect handler }}} */

void server_init(void)
{
    /* handle starting and stopping webserver based on wifi connection */
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                               &disconnect_handler, &server));

    /* setup stepper motor control */
    stepper_setup_gpio();
    stepper_setup_timer();
    stepper_enqueue(0, 0, true); // turn off motors
}
