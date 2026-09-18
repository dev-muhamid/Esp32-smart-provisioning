#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "status_led.h"

static const char *TAG = "STATUS_LED";

// One step of a blink pattern: hold the LED at `on` for `ms` milliseconds.
typedef struct {
    uint8_t  on;
    uint16_t ms;
} led_step_t;

typedef struct {
    const led_step_t *steps;
    uint8_t count;
} led_pattern_t;

static const led_step_t steps_off[]          = {{0, 1000}};
static const led_step_t steps_solid[]        = {{1, 1000}};
static const led_step_t steps_provisioning[] = {{1, 500}, {0, 500}};
static const led_step_t steps_prov_client[]  = {{1, 120}, {0, 180}, {1, 120}, {0, 900}};
static const led_step_t steps_creds_saved[]  = {{1, 70},  {0, 70}};
static const led_step_t steps_connecting[]   = {{1, 150}, {0, 150}};
static const led_step_t steps_no_internet[]  = {{1, 1200},{0, 120}};
static const led_step_t steps_idle[]         = {{1, 50},  {0, 2950}};
static const led_step_t steps_error[]        = {{1, 100}, {0, 100}, {1, 100}, {0, 100}, {1, 100}, {0, 800}};

#define PATTERN(arr) { .steps = (arr), .count = sizeof(arr) / sizeof((arr)[0]) }

// Indexed by led_state_t.
static const led_pattern_t patterns[] = {
    [LED_STATE_OFF]          = PATTERN(steps_off),
    [LED_STATE_BOOT]         = PATTERN(steps_solid),
    [LED_STATE_PROVISIONING] = PATTERN(steps_provisioning),
    [LED_STATE_PROV_CLIENT]  = PATTERN(steps_prov_client),
    [LED_STATE_CREDS_SAVED]  = PATTERN(steps_creds_saved),
    [LED_STATE_CONNECTING]   = PATTERN(steps_connecting),
    [LED_STATE_CONNECTED]    = PATTERN(steps_solid),
    [LED_STATE_NO_INTERNET]  = PATTERN(steps_no_internet),
    [LED_STATE_IDLE]         = PATTERN(steps_idle),
    [LED_STATE_ERROR]        = PATTERN(steps_error),
};

#define PATTERN_COUNT (sizeof(patterns) / sizeof(patterns[0]))

static volatile led_state_t current_state = LED_STATE_OFF;
static TaskHandle_t led_task_handle = NULL;

static void led_write(uint8_t on) {
    gpio_set_level(STATUS_LED_GPIO, on ? STATUS_LED_ACTIVE_LEVEL : !STATUS_LED_ACTIVE_LEVEL);
}

static void status_led_task(void *pvParameters) {
    (void)pvParameters;
    led_state_t playing = current_state;
    uint8_t step = 0;

    while (1) {
        // A state change restarts the pattern from its first step so the
        // transition is visible immediately.
        if (playing != current_state) {
            playing = current_state;
            step = 0;
        }

        const led_pattern_t *pattern = &patterns[playing];
        const led_step_t *s = &pattern->steps[step];
        led_write(s->on);

        // Sleep for the step duration, but wake early if the state changes.
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(s->ms)) > 0) {
            continue;
        }

        step = (step + 1) % pattern->count;
    }
}

void status_led_init(void) {
    if (led_task_handle != NULL) {
        return;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    led_write(0);

    current_state = LED_STATE_BOOT;
    if (xTaskCreate(status_led_task, "status_led", 2048, NULL, 3, &led_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status LED task");
        led_task_handle = NULL;
        return;
    }

    ESP_LOGI(TAG, "Status LED started on GPIO %d", STATUS_LED_GPIO);
}

void status_led_set(led_state_t state) {
    if (state >= PATTERN_COUNT || patterns[state].steps == NULL) {
        ESP_LOGW(TAG, "Ignoring unknown LED state %d", (int)state);
        return;
    }
    if (state == current_state) {
        return;
    }

    current_state = state;
    if (led_task_handle != NULL) {
        xTaskNotifyGive(led_task_handle);
    }
}
