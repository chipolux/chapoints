// vim: foldmethod=marker:foldmarker={{{,}}}
/*
 * Way cooler and faster RMT based stepper motor control, but not all ESP32
 * chipsets support the sync feature that you need to keep multiple data lines
 * (gpio pins) timed together.
 */
#include "driver/rmt_tx.h"
#include "esp_log.h"

static const int STEPPER_PINS[4] = {12, 13, 14, 27};
static const rmt_symbol_word_t FRONT_STEP[2] = {
    {.level0 = 1, .duration0 = 2, .level1 = 1, .duration1 = 2},
    {.level0 = 0, .duration0 = 2, .level1 = 0, .duration1 = 2},
};
static const rmt_symbol_word_t BACK_STEP[2] = {
    {.level0 = 0, .duration0 = 2, .level1 = 1, .duration1 = 2},
    {.level0 = 1, .duration0 = 2, .level1 = 0, .duration1 = 2},
};

void stepper_setup_rmt() // {{{
{
    // setup basic copy encoders and channels for each gpio pin
    // NOTE encoders are stateful to handle partial data steps due to memory
    //      constraints so we need one for each channel
    ESP_LOGI(TAG, "setting up encoders and channels...");
    rmt_encoder_handle_t encoders[4] = {NULL};
    rmt_channel_handle_t channels[4] = {NULL};
    for (int i = 0; i < 4; i++) {
        // setup encoder
        ESP_LOGI(TAG, "    setting up encoder %i", i);
        rmt_copy_encoder_config_t copy_encoder_config = {};
        ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &encoders[i]));

        // setup channel
        ESP_LOGI(TAG, "    setting up channel %i", i);
        rmt_tx_channel_config_t tx_chan_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .gpio_num = STEPPER_PINS[i],
            .mem_block_symbols = 64,
            // .resolution_hz = 1000, // 1 kHz resolution == 1ms ticks
            .resolution_hz = 1 * 1000 * 1000, // 1 MHz resolution == 0.01us ticks
            .trans_queue_depth = 1,
            .flags.invert_out = i > 1, // invert outputs 2 and 3
        };
        ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &channels[i]));

        // enable channel
        ESP_LOGI(TAG, "    enabling channel %i", i);
        ESP_ERROR_CHECK(rmt_enable(channels[i]));
    }

    // install sync manager
    // NOTE supported on:
    //      esp32c3
    //      esp32c6
    //      esp32h2
    //      esp32p4
    //      esp32s2
    //      esp32s3
    ESP_LOGI(TAG, "setting up sync manager...");
    rmt_sync_manager_handle_t synchro = NULL;
    rmt_sync_manager_config_t synchro_config = {
        .tx_channel_array = channels,
        .array_size = sizeof(channels) / sizeof(channels[0]),
    };
    ESP_ERROR_CHECK(rmt_new_sync_manager(&synchro_config, &synchro));

    // no transmits will start until all channels are queued
    ESP_LOGI(TAG, "scheduling transmits...");
    const rmt_transmit_config_t transmit_config = {.loop_count = 32};
    for (int i = 0; i < 4; i++) {
        // swap FRONT and BACK step to go in reverse
        ESP_LOGI(TAG, "    scheduling transmit on channel %i", i);
        ESP_ERROR_CHECK(rmt_transmit(channels[i], encoders[i], i % 2 ? FRONT_STEP : BACK_STEP,
                                     sizeof(rmt_symbol_word_t) * 2, &transmit_config));
    }

    // block until all transmits have completed
    ESP_LOGI(TAG, "waiting on all transmits...");
    rmt_tx_wait_all_done(channels[0], -1);
} // }}}
