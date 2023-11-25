// vim: foldmethod=marker:foldmarker={{{,}}}
#include "stepper.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <assert.h>

static const char *TAG = "ht-stepper";

// pin definitions for stepper
static const int IN1 = 12;
static const int IN2 = 14;
static const int IN3 = 27;
static const int IN4 = 26;

// timer handles
static gptimer_handle_t TIMER = NULL;
static QueueHandle_t QUEUE = NULL;
typedef struct {
    uint16_t steps;
    int8_t direction;
    bool unlock_at_end; // if all gpio should be turned off at end of steps
} StepPlan_t;

static bool IRAM_ATTR timer_alarm_callback(gptimer_handle_t timer, // {{{
                                           const gptimer_alarm_event_data_t *event_data,
                                           void *user_data)
{
    static int phase = 0;
    static StepPlan_t plan = {.steps = 0, .direction = 0, .unlock_at_end = false};

    if (plan.steps == 0) {
        if (plan.unlock_at_end) {
            gpio_set_level(IN1, 0);
            gpio_set_level(IN2, 0);
            gpio_set_level(IN3, 0);
            gpio_set_level(IN4, 0);
        }
        xQueueReceiveFromISR(QUEUE, (void *)&plan, NULL);
    }

    if (plan.steps > 0) {
        // NOTE: if direction is 0 it is just a way to enqueue a delay
        if (plan.direction != 0) {
            switch (phase) {
            case 0:
                gpio_set_level(IN1, 1);
                gpio_set_level(IN2, 0);
                gpio_set_level(IN3, 0);
                gpio_set_level(IN4, 1);
                break;
            case 1:
                gpio_set_level(IN1, 1);
                gpio_set_level(IN2, 1);
                gpio_set_level(IN3, 0);
                gpio_set_level(IN4, 0);
                break;
            case 2:
                gpio_set_level(IN1, 0);
                gpio_set_level(IN2, 1);
                gpio_set_level(IN3, 1);
                gpio_set_level(IN4, 0);
                break;
            case 3:
                gpio_set_level(IN1, 0);
                gpio_set_level(IN2, 0);
                gpio_set_level(IN3, 1);
                gpio_set_level(IN4, 1);
                break;
            }
            phase = (phase + (plan.direction < 0 ? 3 : 1)) % 4;
        }
        --plan.steps;
    }

    return false;
} // }}}

void stepper_setup_gpio() // {{{
{
    /* setup smoke control gpio and task */
    const unsigned long long mask = (1ULL << IN1 | 1ULL << IN2 | 1ULL << IN3 | 1ULL << IN4);
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE; /* disable interrupts */
    io_conf.mode = GPIO_MODE_OUTPUT;       /* set to output mode */
    io_conf.pin_bit_mask = mask;           /* pin bit mask */
    io_conf.pull_down_en = 0;              /* disable pull-down mode */
    io_conf.pull_up_en = 0;                /* disable pull-up mode */
    gpio_config(&io_conf);
    gpio_set_level(IN1, 0);
    gpio_set_level(IN2, 0);
    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 0);
} // }}}

void stepper_setup_timer() // {{{
{
    if (QUEUE == NULL) {
        ESP_LOGI(TAG, "setting up queue...");
        QUEUE = xQueueCreate(20, sizeof(StepPlan_t));
        assert(QUEUE != NULL);
    }

    if (TIMER == NULL) {
        ESP_LOGI(TAG, "setting up timer...");
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            // NOTE: 1000 fails a clock divider assertion...
            .resolution_hz = 10 * 1000, // 10kHz, 1 tick = 0.1ms
        };
        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &TIMER));

        ESP_LOGI(TAG, "setting up timer alarm...");
        gptimer_alarm_config_t alarm_config = {
            // NOTE: about 15 (3ms pulses on each phase) is the fastest (9v)
            //       20 is the max according to spec (4ms pulses on each phase)
            .alarm_count = 20, // with 10kHz resolution
            .reload_count = 0, // set count back to 0 on alarm
            .flags.auto_reload_on_alarm = true,
        };
        ESP_ERROR_CHECK(gptimer_set_alarm_action(TIMER, &alarm_config));

        ESP_LOGI(TAG, "setting up timer alarm callbacks...");
        gptimer_event_callbacks_t callbacks = {
            .on_alarm = timer_alarm_callback,
        };
        ESP_ERROR_CHECK(gptimer_register_event_callbacks(TIMER, &callbacks, NULL));

        ESP_LOGI(TAG, "enabling and starting timer...");
        ESP_ERROR_CHECK(gptimer_enable(TIMER));
        ESP_ERROR_CHECK(gptimer_start(TIMER));
    }
} // }}}

void stepper_teardown_timer() // {{{
{
    if (TIMER != NULL) {
        ESP_LOGI(TAG, "tearing down timer...");
        ESP_ERROR_CHECK(gptimer_stop(TIMER));
        ESP_ERROR_CHECK(gptimer_disable(TIMER));
        ESP_ERROR_CHECK(gptimer_del_timer(TIMER));
        TIMER = NULL;
    }

    if (QUEUE != NULL) {
        ESP_LOGI(TAG, "tearing down queue...");
        vQueueDelete(QUEUE);
    }
} // }}}

// NOTE: count = steps to take
//       direction is + forward, - backward, 0 delay
//       unlock_at_end is if gpio should all go low after steps taken
bool stepper_enqueue(const uint16_t count, const int8_t direction,
                     const bool unlock_at_end) // {{{
{
    if (QUEUE == NULL) {
        return false;
    }
    ESP_LOGI(TAG, "enqueueing step plan with %i steps", count);
    StepPlan_t plan = {.steps = count, .direction = direction, .unlock_at_end = unlock_at_end};
    return xQueueSendToBack(QUEUE, (void *)&plan, 0);
} // }}}
