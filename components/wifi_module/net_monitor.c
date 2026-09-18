#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"
#include "net_monitor.h"
#include "status_led.h"

static const char *TAG = "NET_MON";

typedef enum {
    LINK_UNKNOWN = 0,
    LINK_ONLINE,
    LINK_OFFLINE,
} link_status_t;

static esp_ping_handle_t ping_handle = NULL;
static link_status_t link_status = LINK_UNKNOWN;
static int consecutive_failures = 0;

static void set_link_status(link_status_t status) {
    if (status == link_status) {
        return;
    }
    link_status = status;

    if (status == LINK_ONLINE) {
        ESP_LOGI(TAG, "Internet reachable");
        status_led_set(LED_STATE_CONNECTED);
    } else {
        ESP_LOGW(TAG, "Internet unreachable (%d consecutive misses)", consecutive_failures);
        status_led_set(LED_STATE_NO_INTERNET);
    }
}

static void on_ping_success(esp_ping_handle_t hdl, void *args) {
    (void)args;
    uint32_t elapsed_ms = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_ms, sizeof(elapsed_ms));

    consecutive_failures = 0;
    if (link_status != LINK_ONLINE) {
        ESP_LOGI(TAG, "Reply from %s in %" PRIu32 " ms", NET_MONITOR_TARGET_IP, elapsed_ms);
    }
    set_link_status(LINK_ONLINE);
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args) {
    (void)hdl;
    (void)args;

    if (consecutive_failures < NET_MONITOR_FAIL_THRESHOLD) {
        consecutive_failures++;
    }
    ESP_LOGW(TAG, "No reply from %s (%d/%d)", NET_MONITOR_TARGET_IP,
             consecutive_failures, NET_MONITOR_FAIL_THRESHOLD);

    if (consecutive_failures >= NET_MONITOR_FAIL_THRESHOLD) {
        set_link_status(LINK_OFFLINE);
    }
}

void net_monitor_start(void) {
    if (ping_handle != NULL) {
        ESP_LOGI(TAG, "Monitor already running");
        return;
    }

    ip_addr_t target;
    if (!ipaddr_aton(NET_MONITOR_TARGET_IP, &target)) {
        ESP_LOGE(TAG, "Invalid target address '%s'", NET_MONITOR_TARGET_IP);
        return;
    }

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr = target;
    config.count = ESP_PING_COUNT_INFINITE;
    config.interval_ms = NET_MONITOR_INTERVAL_MS;
    config.timeout_ms = NET_MONITOR_TIMEOUT_MS;
    config.data_size = 32;
    // The callbacks log and poke the LED task, so give the ping task a
    // little more room than the 2 KB default.
    config.task_stack_size = 3072;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL,   // never fires: this is an infinite session
        .cb_args = NULL,
    };

    esp_err_t err = esp_ping_new_session(&config, &cbs, &ping_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ping session: %s", esp_err_to_name(err));
        ping_handle = NULL;
        return;
    }

    link_status = LINK_UNKNOWN;
    consecutive_failures = 0;

    err = esp_ping_start(ping_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start ping session: %s", esp_err_to_name(err));
        esp_ping_delete_session(ping_handle);
        ping_handle = NULL;
        return;
    }

    ESP_LOGI(TAG, "Monitoring %s every %d ms", NET_MONITOR_TARGET_IP, NET_MONITOR_INTERVAL_MS);
}

void net_monitor_stop(void) {
    if (ping_handle == NULL) {
        return;
    }

    esp_ping_stop(ping_handle);
    esp_ping_delete_session(ping_handle);
    ping_handle = NULL;
    link_status = LINK_UNKNOWN;
    consecutive_failures = 0;

    ESP_LOGI(TAG, "Monitor stopped");
}

bool net_monitor_is_online(void) {
    return link_status == LINK_ONLINE;
}
