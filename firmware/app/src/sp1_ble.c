/*
 * Bluetooth MIDI on the SP-1's own radio (the nRF52840's, not the CYBT module's).
 *
 * The SP-1 advertises as "SP-1 SCQ xxxx" (xxxx from the chip's id) with the standard
 * BLE-MIDI service, so macOS (Audio MIDI Setup > Bluetooth, or the editor's "pair" window)
 * and Windows see it as a MIDI port. Sleepcousonique talks over it in System Exclusive:
 * every message the editor sends is F0 7D 'S' 'Q' <command> ... F7 (sp1_scq.c reads them).
 *
 * Threads: GATT writes arrive on the Bluetooth RX thread; finished messages are queued
 * here and taken by the main loop (sp1_ble_take). Notifications go out from the main loop.
 */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/ring_buffer.h>
#include <stdio.h>
#include <string.h>

#include "sp1_ble.h"

/* BLE-MIDI: service 03B80E5A-EDE8-4B33-A751-6CE34EC4C700, characteristic 7772E5DB-3868-4112-A1A9-F2669D106BF3 */
#define MIDI_SVC  BT_UUID_128_ENCODE(0x03b80e5a, 0xede8, 0x4b33, 0xa751, 0x6ce34ec4c700)
#define MIDI_CHR  BT_UUID_128_ENCODE(0x7772e5db, 0x3868, 0x4112, 0xa1a9, 0xf2669d106bf3)
static const struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(MIDI_SVC);
static const struct bt_uuid_128 chr_uuid = BT_UUID_INIT_128(MIDI_CHR);

static struct bt_conn *conn;
static bool enabled, notify_on, started;
static char name[24];

/* finished SysEx messages, each stored as [len hi][len lo][bytes...] */
RING_BUF_DECLARE(msgs, 3072);
static uint8_t sx[SP1_BLE_MAX_MSG];
static int sx_n = -1;                  /* -1: not inside a SysEx */

static void sysex_done(void)
{
	if (sx_n > 0 && ring_buf_space_get(&msgs) >= (uint32_t)sx_n + 2u) {
		uint8_t h[2] = { (uint8_t)(sx_n >> 8), (uint8_t)sx_n };
		ring_buf_put(&msgs, h, 2);
		ring_buf_put(&msgs, sx, (uint32_t)sx_n);
	}
	sx_n = -1;
}

/* A BLE-MIDI packet: a header byte, then timestamped MIDI; a SysEx may run across packets
 * (continuation packets carry its data straight after the header). Only SysEx matters here. */
static ssize_t midi_write(struct bt_conn *c, const struct bt_gatt_attr *attr, const void *buf,
			  uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(c); ARG_UNUSED(attr); ARG_UNUSED(offset); ARG_UNUSED(flags);
	const uint8_t *p = buf;
	if (len < 2 || !(p[0] & 0x80)) {
		return len;
	}
	for (int i = 1; i < len; i++) {
		uint8_t b = p[i];
		if (sx_n >= 0) {
			if (b < 0x80) {
				if (sx_n < (int)sizeof sx) {
					sx[sx_n++] = b;
				}
			} else if (b == 0xF7 && !(i + 1 < len && p[i + 1] == 0xF7)) {
				sysex_done();
			}                               /* else a timestamp: skip */
		} else if (b == 0xF0) {
			sx_n = 0;
		}
	}
	return len;
}

static ssize_t midi_read(struct bt_conn *c, const struct bt_gatt_attr *attr, void *buf,
			 uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(c, attr, buf, len, offset, NULL, 0);   /* reads are empty */
}

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	notify_on = value == BT_GATT_CCC_NOTIFY;
}

BT_GATT_SERVICE_DEFINE(midi_svc,
	BT_GATT_PRIMARY_SERVICE(&svc_uuid),
	BT_GATT_CHARACTERISTIC(&chr_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, midi_read, midi_write, NULL),
	BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, MIDI_SVC),
};
static struct bt_data sd[1];

static void advertise(void)
{
	if (!enabled || conn) {
		return;
	}
	sd[0] = (struct bt_data)BT_DATA(BT_DATA_NAME_COMPLETE, name, (uint8_t)strlen(name));
	int err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2,
						  BT_GAP_ADV_FAST_INT_MAX_2, NULL),
				  ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err && err != -EALREADY) {
		printk("BLE advertising failed (%d)\n", err);
	}
}

static void connected(struct bt_conn *c, uint8_t err)
{
	if (err) {
		return;
	}
	conn = bt_conn_ref(c);
	sx_n = -1;
	printk("BLE connected\n");
}

static void disconnected(struct bt_conn *c, uint8_t reason)
{
	ARG_UNUSED(c);
	if (conn) {
		bt_conn_unref(conn);
		conn = NULL;
	}
	notify_on = false;
	printk("BLE disconnected (%u)\n", reason);
}

static void recycled(void)
{
	advertise();                         /* the connection is gone: be found again */
}

BT_CONN_CB_DEFINE(conn_cbs) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

void sp1_ble_set(bool on)
{
	enabled = on;
	if (!on) {
		(void)bt_le_adv_stop();
		if (conn) {
			(void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		}
		printk("BLE off\n");
		return;
	}
	if (!started) {
		int err = bt_enable(NULL);
		if (err) {
			printk("BLE could not start (%d)\n", err);
			enabled = false;
			return;
		}
		started = true;
		uint8_t id[8] = { 0 };
		ssize_t n = hwinfo_get_device_id(id, sizeof id);
		unsigned tag = n >= 2 ? ((unsigned)id[n - 2] << 8) | id[n - 1] : 0u;
		snprintf(name, sizeof name, "SP-1 SCQ %04x", tag & 0xffffu);
		(void)bt_set_name(name);
	}
	advertise();
	printk("BLE on as \"%s\"\n", name);
}

bool sp1_ble_on(void) { return enabled; }
bool sp1_ble_connected(void) { return conn != NULL; }
const char *sp1_ble_name(void) { return name; }

int sp1_ble_take(uint8_t *out, int cap)
{
	uint8_t h[2];
	if (ring_buf_get(&msgs, h, 2) != 2) {
		return 0;
	}
	int n = (h[0] << 8) | h[1];
	int keep = n < cap ? n : cap;
	ring_buf_get(&msgs, out, (uint32_t)keep);
	for (int i = keep; i < n; i++) {     /* too long for the caller: drop the rest */
		uint8_t d;
		ring_buf_get(&msgs, &d, 1);
	}
	return keep;
}

/* F0 <payload> F7 as BLE-MIDI notifications, split to the connection's MTU */
void sp1_ble_send(const uint8_t *payload, int n)
{
	if (!conn || !notify_on) {
		return;
	}
	const int room = (int)bt_gatt_get_mtu(conn) - 3;
	uint8_t pkt[247];
	const int cap = room < (int)sizeof pkt ? room : (int)sizeof pkt;
	uint16_t ts = (uint16_t)(k_uptime_get_32() & 0x1fff);
	const uint8_t hdr = (uint8_t)(0x80 | ((ts >> 7) & 0x3f)), tsl = (uint8_t)(0x80 | (ts & 0x7f));
	int i = 0, k = 0;
	pkt[k++] = hdr; pkt[k++] = tsl; pkt[k++] = 0xF0;
	for (;;) {
		while (i < n && k < cap - 2) {
			pkt[k++] = payload[i++] & 0x7f;
		}
		if (i >= n) {
			pkt[k++] = tsl; pkt[k++] = 0xF7;
			(void)bt_gatt_notify(conn, &midi_svc.attrs[1], pkt, (uint16_t)k);
			return;
		}
		(void)bt_gatt_notify(conn, &midi_svc.attrs[1], pkt, (uint16_t)k);
		k = 0;
		pkt[k++] = hdr;               /* a continuation: header, then data */
	}
}
