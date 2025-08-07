// Libraries used
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "freertos/semphr.h"
#include "freertos/idf_additions.h"

// Macros
#define TRIG_pin_num 16
#define ECHO_pin_num 18
#define INTERNAL_RGB_LED 38

// Global structs, handles, and variables
typedef struct {
    SemaphoreHandle_t RMT_semphr;
    uint16_t distance;
} user_ctx_RMT_t;

static TaskHandle_t distanceSensor_handle = NULL;
static TaskHandle_t dcMotors_handle = NULL;

static const char* printerTask = "printer"; // TO BE DELETED AFTER DEBUGGING: used to print out msgs to the terminal

/*
* TODO:
*   - Cosmetic: Have a space for args in-between parathesis: function( arg ){...}
*   - Create a header file
*/

// Callback to be invoked when ECHO reception is done. IRAM_ATTR is needed for ISRs
// in order to place them in the internal RAM for quick triggering
// Return: Whether a high priority task has been waken up by this callback function 
static IRAM_ATTR bool echoCallBack(rmt_channel_handle_t rx_chan, const rmt_rx_done_event_data_t *edata, void *user_ctx){
    // Get the signaling semaphore
    SemaphoreHandle_t RMT_semphr = (SemaphoreHandle_t) ((user_ctx_RMT_t*)user_ctx)->RMT_semphr;
    
    // Convert the HIGH level duration into centimeters AND
    // Save the data(no printing in ISR as it uses internal locking)
    ((user_ctx_RMT_t*)user_ctx)->distance = edata->received_symbols->duration0 / 58;

    // Release the semaphor to signal that a single ECHO signal has been analyzed.
    xSemaphoreGiveFromISR( RMT_semphr, pdFALSE );

    return pdFALSE;
}

void distanceSensor_task( void* pvParameters ){
    // - Pins -
    // Reset the pins
    ESP_ERROR_CHECK( gpio_reset_pin(TRIG_pin_num) );
    ESP_ERROR_CHECK( gpio_reset_pin(ECHO_pin_num) );
    ESP_ERROR_CHECK( gpio_reset_pin(INTERNAL_RGB_LED) );


    // Set direction for TRIG and ECHO pins
    ESP_ERROR_CHECK( gpio_set_direction(TRIG_pin_num, GPIO_MODE_OUTPUT) );
    ESP_ERROR_CHECK( gpio_set_direction(ECHO_pin_num, GPIO_MODE_INPUT) );
    ESP_ERROR_CHECK( gpio_set_direction(INTERNAL_RGB_LED, GPIO_MODE_OUTPUT) );

    // - RMT -
    // Define RMT for ECHO
    rmt_channel_handle_t rx_chan;

    rmt_rx_channel_config_t rx_chan_config = {0};
    rx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;   // select source clock
    rx_chan_config.resolution_hz = 1 * 1000 * 1000; // 1 MHz tick resolution, i.e., 1 tick = 1 µs
    rx_chan_config.mem_block_symbols = 48;         // memory block size, 48 * 4 Bytes
    rx_chan_config.gpio_num = ECHO_pin_num;        // GPIO number
    rx_chan_config.flags.invert_in = false;        // do not invert input signal
    rx_chan_config.flags.with_dma = false;          // do not need DMA backend
    rx_chan_config.intr_priority = 3;               // priority

    // Create the RX RMT channel
    ESP_ERROR_CHECK( rmt_new_rx_channel(&rx_chan_config, &rx_chan) );

    // Initialize the semaphore to stall the main loop until the pulseIn received
    SemaphoreHandle_t RMT_semphr = xSemaphoreCreateBinary();
    assert(RMT_semphr != NULL);

    // Initialize RMT struct for user_ctx of the event callback
    user_ctx_RMT_t user_ctx_RMT = {
        .RMT_semphr = RMT_semphr,
        .distance = 0,
    };

    // Register a callback function for RMT ISR
    rmt_rx_event_callbacks_t RMTevent_struct = {
        .on_recv_done = echoCallBack
    };

    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_chan, &RMTevent_struct, &user_ctx_RMT));

    // Enabel RMT
    ESP_ERROR_CHECK(rmt_enable(rx_chan));

    // Initialize the receive config (distance = duration in micro seconds / 58) for RMT
    rmt_receive_config_t rx_rec_config = {
        .signal_range_min_ns = 3187, // min available value
        .signal_range_max_ns = 11600000, // max 200 cm
    };

    // Initialize a buffer for RMT to read ECHO duration that is forced to be 8-bytes aligned on the stack
    rmt_symbol_word_t pulseIn_buffer[2] __attribute__((aligned(8)));

    // Main loop
    while( true ) {
        // Short pause to avoid starvation
        vTaskDelay( 100 / portTICK_PERIOD_MS );

        // Initiate the RX transaction for RMT
        ESP_ERROR_CHECK( rmt_receive( rx_chan, pulseIn_buffer, sizeof(pulseIn_buffer), &rx_rec_config) );

        // Send a HIGH signal for 10 micro seconds via TRIG
        ESP_ERROR_CHECK( gpio_set_level(TRIG_pin_num, 1) );
        vTaskDelay(10 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK( gpio_set_level(TRIG_pin_num, 0) );

        // Wait for the ECHO output
        if( xSemaphoreTake( RMT_semphr, portMAX_DELAY ) == pdTRUE ){
            ESP_LOGI( printerTask, "Distance = %d", user_ctx_RMT.distance);


            // TO BE DELETED AFTER DEBUGGING: Turn on the on-board RGB LED when there is a drone above the sensor
            if( user_ctx_RMT.distance <= 7 ){
                ESP_LOGI( printerTask, "RGB ON");
            }
        }
    }
}

void dcMotors_task( void* pvParameters ){
    while( true ) {
        // Placeholder...
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGI( printerTask, "DC Motors task is running..." );
    }
}


/* * * * *
 * Main  *
 * * * * */
void app_main(void)
{
    // Define a task dedicated to the control of ultrasonic distance sensor
    xTaskCreatePinnedToCore(
        distanceSensor_task, 
        "DistanceSensor", 
        4096, // memory size in bytes
        NULL, // parameter to pass to function 
        1, // priority
        &distanceSensor_handle, // task handle
        1 // core ID
    );

    // Define a task dedicated to the control of DC motors
    xTaskCreatePinnedToCore(
        dcMotors_task, 
        "DC_motors", 
        2048, // memory size in bytes
        NULL, // parameter to pass to function 
        1, // priority
        &dcMotors_handle, // task handle
        1 // core ID
    );
}
