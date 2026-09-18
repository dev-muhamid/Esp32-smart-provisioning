#ifndef STATUS_LED_H
#define STATUS_LED_H

/**
 * Indicator LED driver.
 *
 * A single GPIO is driven by a background task that plays a repeating
 * blink pattern. Each connectivity state owns one pattern, so the device
 * can be diagnosed without a serial monitor attached.
 *
 *   State                  Pattern                  Meaning
 *   ---------------------  -----------------------  -----------------------------
 *   LED_STATE_BOOT         Solid on                 Stack coming up
 *   LED_STATE_PROVISIONING Even 1 Hz blink          AP + BLE up, waiting for creds
 *   LED_STATE_PROV_CLIENT  Double blink, pause      Phone joined the AP / BLE link
 *   LED_STATE_CREDS_SAVED  Rapid flutter            Credentials stored, restarting
 *   LED_STATE_CONNECTING   Fast 3 Hz blink          Associating with the router
 *   LED_STATE_CONNECTED    Solid on                 Got an IP, internet reachable
 *   LED_STATE_NO_INTERNET  Solid on, short gaps     Got an IP, DNS lookup failed
 *   LED_STATE_IDLE         Brief blip every 3 s     Provisioning timed out, radio off
 *   LED_STATE_ERROR        Triple blink, pause      Unrecoverable failure
 */

// GPIO12 on the ESP32 is the MTDI strapping pin: a HIGH level at reset
// selects a 1.8 V VDD_SDIO and the module will not boot. Wire the LED
// active-high (GPIO -> resistor -> LED -> GND) so the pin is never pulled
// up externally.
#define STATUS_LED_GPIO           12
#define STATUS_LED_ACTIVE_LEVEL   1

typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_BOOT,
    LED_STATE_PROVISIONING,
    LED_STATE_PROV_CLIENT,
    LED_STATE_CREDS_SAVED,
    LED_STATE_CONNECTING,
    LED_STATE_CONNECTED,
    LED_STATE_NO_INTERNET,
    LED_STATE_IDLE,
    LED_STATE_ERROR,
} led_state_t;

/**
 * @brief Configure the LED GPIO and start the blink task.
 *
 * Safe to call more than once; subsequent calls are ignored.
 */
void status_led_init(void);

/**
 * @brief Switch to the pattern for @p state.
 *
 * Takes effect immediately (the blink task is woken), and is safe to call
 * from any task, including event handler and NimBLE host contexts.
 */
void status_led_set(led_state_t state);

#endif
