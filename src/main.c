#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define CONFIG_BT_DEVICE_NAME "Special Course Peripheral Server"
#define BT_DEVICE_NAME_FULL CONFIG_BT_DEVICE_NAME
#define BT_DEVICE_NAME_SHORT "Special Course"

/* Advertising payload:
 * - Device Name
 * - Flags
 * - Immediate Alert Service UUID: 0x1802
 * - Volume Control Service UUID: 0x1844
 */
static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_NAME_SHORTENED, BT_DEVICE_NAME_SHORT),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x02, 0x18, 0x44, 0x18), /* 0x1802 IAS, 0x1844 VCS*/
};

/* Use LED0 from the board's devicetree alias */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static struct k_work_delayable blink_work;
static struct k_work adv_start_work;
static uint8_t alert_level;

struct vcs_state {
	uint8_t volume_setting;  // current volume
	uint8_t mute;            // 0 or 1
	uint8_t change_counter;  // increment on change
};

static struct vcs_state volume_state = {
	.volume_setting = 1,  // initial volume
	.mute = 2,             // not muted
	.change_counter = 3,   // no changes
};

static void led_off(void)  { gpio_pin_set_dt(&led, 0); }
static void led_on(void)   { gpio_pin_set_dt(&led, 1); }

static void adv_start_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	printk("Starting advertisement\n");
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (%d)\n", err);
	} else {
		printk("Advertising as connectable peripheral\n");
	}
}

/* Simple blinker for "Mild Alert" */
static void blink_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	static bool s;
	s = !s;
	gpio_pin_set_dt(&led, s);
	/* Slow blink for mild alert */
	k_work_schedule(&blink_work, K_MSEC(500));
}

static void stop_blink(void)
{
	k_work_cancel_delayable(&blink_work);
	led_off();
}

static void start_blink(void)
{
	k_work_schedule(&blink_work, K_NO_WAIT);
}

/* GATT write handler for Alert Level (0x2A06) */
static ssize_t write_alert(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	if (offset != 0 || len != 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t level = *((const uint8_t *)buf);
        printk("Alert Level write received: %d\n", level);

	/* Spec: 0x00=No Alert, 0x01=Mild, 0x02=High; others not allowed */
	if (level > 0x02) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	alert_level = level;

	switch (alert_level) {
                case 0x00: /* No Alert */
                        stop_blink();
                        break;
                case 0x01: /* Mild Alert -> slow blink */
                        start_blink();
                        break;
                case 0x02: /* High Alert -> solid on */
                        stop_blink();
                        led_on();
                        break;
	}

	return len;
}

/* GATT read handler for Volume State (0x2B7D) */
static ssize_t read_volume_state(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &volume_state, sizeof(volume_state));
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
        printk("Resuming advertising\n");
        k_work_submit(&adv_start_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

/* GATT: Primary service + single writable characteristic */
BT_GATT_SERVICE_DEFINE(ias_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_IAS),
	BT_GATT_CHARACTERISTIC(BT_UUID_ALERT_LEVEL,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, write_alert, &alert_level),
);

BT_GATT_SERVICE_DEFINE(vcs_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_VCS),
	BT_GATT_CHARACTERISTIC(BT_UUID_VCS_STATE,
			       BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ,
			       read_volume_state, NULL, &volume_state),
);

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (%d)\n", err);
		return;
	}

	printk("Bluetooth ready, starting advertising\n");

	k_work_submit(&adv_start_work);
}

int main(void)
{
	int err;

	if (!device_is_ready(led.port)) {
		printk("LED device not ready\n");
		return 0;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	k_work_init_delayable(&blink_work, blink_handler);
	k_work_init(&adv_start_work, adv_start_handler);

	err = bt_enable(bt_ready);
	if (err) {
		printk("bt_enable failed (%d)\n", err);
		return 0;
	}

	for (;;) {
		k_sleep(K_SECONDS(1));
	}
}
