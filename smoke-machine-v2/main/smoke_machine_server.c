#include "smoke_machine_server.h"

#include <stdlib.h>
#include <sys/param.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <esp_http_server.h>

/* GPIO4 high presses smoke button, low releases it */
#define SMOKE_PIN 4
#define SMOKE_MASK (1ULL << SMOKE_PIN)

static const char *TAG = "sm-server";

static httpd_handle_t server = NULL;

/* flags and status values for controlling smoke */
static bool smoke_should_activate = false;
static bool smoke_should_deactivate = false;
static bool smoke_is_active = false;
static int smoke_secs_left = 0;

esp_err_t root_get_handler(httpd_req_t *req)
{
    const char resp_str[] =
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<style>"
        "html, body { margin: 10px; padding: 0; }"
        "body { display: flex; flex-direction: column; }"
        "button, input { margin: 5px; padding: 5px; background: orange; font-size: 2em; }"
        "</style>"
        "<script type=\"text/javascript\">"
        "function activateSmoke() {"
        "var duration = document.getElementById(\"duration\").value;"
        "fetch(`/activate?duration=${duration}`, {method: \"POST\"});"
        "}"
        "function deactivateSmoke() {"
        "fetch(\"/deactivate\", {method: \"POST\"});"
        "}"
        "</script>"
        "</head>"
        "<button onclick=\"activateSmoke()\">Activate Smoke</button>"
        "<input type=\"number\" id=\"duration\" min=\"1\" max=\"90\" value=\"15\">"
        "<button onclick=\"deactivateSmoke()\">Deactivate Smoke</button>"
        "</html>";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}
httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL};

esp_err_t activate_post_handler(httpd_req_t *req)
{
    /* read url query string length and alloc memory for it (+1 for null) */
    char *buf;
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "activate with query: %s", buf);
            char param[32];
            if (httpd_query_key_value(buf, "duration", param, sizeof(param)) == ESP_OK) {
                int duration = atoi(param);
                if (duration < 1 || duration > 90) {
                    /* default to 30 second burst */
                    duration = 30;
                }
                ESP_LOGI(TAG, "parsed duration: %d", duration);
                if (!smoke_is_active) {
                    smoke_should_activate = true;
                    smoke_secs_left = duration;
                }
            }
        }
        free(buf);
    }

    const char resp_str[] = "activated";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}
httpd_uri_t activate = {
    .uri = "/activate", .method = HTTP_POST, .handler = activate_post_handler, .user_ctx = NULL};

esp_err_t deactivate_post_handler(httpd_req_t *req)
{
    if (smoke_is_active) {
        smoke_should_deactivate = true;
        smoke_secs_left = 0;
    }

    const char resp_str[] = "deactivated";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}
httpd_uri_t deactivate = {.uri = "/deactivate",
                          .method = HTTP_POST,
                          .handler = deactivate_post_handler,
                          .user_ctx = NULL};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "starting on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "registering uri handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &activate);
        httpd_register_uri_handler(server, &deactivate);
        return server;
    }

    ESP_LOGW(TAG, "error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server) { httpd_stop(server); }

static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server) {
        ESP_LOGI(TAG, "stopping webserver, wifi disconnected");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                            void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "starting webserver, wifi connected");
        *server = start_webserver();
    }
}

void smoke_loop(void *pv_parameters)
{
    /* start loop to control smoke */
    while (true) {
        if (smoke_is_active && smoke_should_deactivate) {
            gpio_set_level(SMOKE_PIN, 0);
            smoke_is_active = false;
            smoke_should_activate = false;
            smoke_should_deactivate = false;
            ESP_LOGI(TAG, "smoke deactivated, forced");
        } else if (!smoke_is_active && smoke_should_activate) {
            gpio_set_level(SMOKE_PIN, 1);
            smoke_is_active = true;
            smoke_should_activate = false;
            smoke_should_deactivate = false;
            ESP_LOGI(TAG, "smoke activated for %d seconds", smoke_secs_left);
        } else if (smoke_is_active && smoke_secs_left <= 0) {
            gpio_set_level(SMOKE_PIN, 0);
            smoke_is_active = false;
            smoke_should_activate = false;
            smoke_should_deactivate = false;
            smoke_secs_left = 0;
            ESP_LOGI(TAG, "smoke deactivated, time up");
        }
        /* NOTE: other tasks, wifi, webserver, etc. will slightly inflate the
         *       delay so it won't activate for EXACTLY the seconds provided
         *       but it should be within +-10 milliseconds
         */
        smoke_secs_left = MAX(0, smoke_secs_left - 1);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void smoke_machine_server_init(void)
{
    /* handle starting and stopping webserver based on wifi connection */
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                               &disconnect_handler, &server));

    /* setup smoke control gpio and task */
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE; /* disable interrupts */
    io_conf.mode = GPIO_MODE_OUTPUT;       /* set to output mode */
    io_conf.pin_bit_mask = SMOKE_MASK;     /* pin bit mask */
    io_conf.pull_down_en = 0;              /* disable pull-down mode */
    io_conf.pull_up_en = 0;                /* disable pull-up mode */
    gpio_config(&io_conf);
    gpio_set_level(SMOKE_PIN, 0);
    xTaskCreate(&smoke_loop, "smoke_loop", 2048, NULL, 1, NULL);

    /* setup webserver */
    server = start_webserver();
}
