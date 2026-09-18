#ifndef NET_MONITOR_H
#define NET_MONITOR_H

#include <stdbool.h>

/**
 * Runtime internet reachability monitor.
 *
 * Sends one ICMP echo request to a fixed IP address every
 * NET_MONITOR_INTERVAL_MS and drives the indicator LED between
 * LED_STATE_CONNECTED and LED_STATE_NO_INTERNET.
 *
 * The target is a literal IP rather than a hostname so that a DNS outage
 * and a connectivity outage stay distinguishable, and so no blocking name
 * lookup is needed. Probing runs in the ping component's own task, so
 * nothing here blocks the caller.
 */

// Target must be an IP literal; a resolver is deliberately not used.
#define NET_MONITOR_TARGET_IP     "8.8.8.8"
#define NET_MONITOR_INTERVAL_MS   10000
#define NET_MONITOR_TIMEOUT_MS    3000

// Consecutive missed replies before the WAN is declared down. Guards
// against a single dropped packet flapping the LED.
#define NET_MONITOR_FAIL_THRESHOLD 3

/**
 * @brief Begin probing. Call once the station has an IP address.
 *
 * Safe to call repeatedly; an already-running monitor is left alone.
 */
void net_monitor_start(void);

/**
 * @brief Stop probing and release the ping session.
 *
 * Call when the station loses its IP or the radio is shut down.
 */
void net_monitor_stop(void);

/**
 * @brief Last known reachability result.
 *
 * False until the first successful reply, so a caller cannot mistake
 * "not probed yet" for "online".
 */
bool net_monitor_is_online(void);

#endif
