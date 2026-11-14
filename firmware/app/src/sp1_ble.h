/*
 * Bluetooth MIDI on the SP-1's own radio. See sp1_ble.c. Main thread only.
 */
#ifndef SP1_BLE_H
#define SP1_BLE_H

#include <stdint.h>
#include <stdbool.h>

#define SP1_BLE_MAX_MSG 512            /* the longest SysEx kept (a data chunk of a dream) */

void sp1_ble_set(bool on);             /* advertise (and start the radio the first time), or stop */
bool sp1_ble_on(void);
bool sp1_ble_connected(void);
const char *sp1_ble_name(void);        /* "SP-1 SCQ xxxx" */

/* the next SysEx that arrived, without F0 / F7: its length, or 0 if none */
int  sp1_ble_take(uint8_t *out, int cap);

/* send F0 <payload> F7 (payload bytes must be 7-bit) */
void sp1_ble_send(const uint8_t *payload, int n);

#endif
