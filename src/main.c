#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define BT_DEVICE_NAME_FULL CONFIG_BT_DEVICE_NAME
#define BT_DEVICE_NAME_SHORT "Renderer"

/* Advertising payload:
 * - Device Name
 * - Flags
 * - Volume Control Service UUID: 0x1844
 */
static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_NAME_SHORTENED, BT_DEVICE_NAME_SHORT),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x44, 0x18), /* 0x1844 VCS (little-endian)*/
};

static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static struct k_work adv_start_work;

struct vcs_volume_state {
	uint8_t volume_setting;  // current volume
	uint8_t mute;            // 0 or 1
	uint8_t change_counter;  // increment on change
};

static struct vcs_volume_state volume_state = {
	.volume_setting = 128,  // initial volume
	.mute = 0,             // not muted
	.change_counter = 0,   // no changes
};

//static void status_led_off(void) { gpio_pin_set_dt(&status_led, 0); }
//static void status_led_on(void)  { gpio_pin_set_dt(&status_led, 1); }

static void adv_start_handler(struct k_work *work)
{
	(void)(work);

	printk("Starting advertisement\n");
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (%d)\n", err);
	} else {
		printk("Advertising as connectable peripheral\n");
	}
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

/* GATT: Primary service */
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

	if (!device_is_ready(status_led.port)) {
		printk("Status LED device not ready\n");
		return 0;
	}

	gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_ACTIVE);

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