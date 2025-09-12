#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "VolumeControlService.h"

#define BT_DEVICE_NAME_FULL CONFIG_BT_DEVICE_NAME
#define BT_DEVICE_NAME_SHORT "Renderer"

#define MAX_CONNECTED_CLIENTS 5

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

static const struct gpio_dt_spec info_button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static struct k_work adv_start_work;
static struct gpio_callback button_cb_data;

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
	k_work_submit(&adv_start_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (%d)\n", err);
		return;
	}

	printk("Bluetooth ready, starting advertising\n");

	k_work_submit(&adv_start_work);
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("Volume State:\n");
	printk("  Volume Setting: %d\n", volume_state.volume_setting);
	printk("  Mute: %d\n", volume_state.mute);
	printk("  Change Counter: %d\n", volume_state.change_counter);
}

/* GATT: Primary service */
BT_GATT_SERVICE_DEFINE(vcs_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_VCS),
	BT_GATT_CHARACTERISTIC(BT_UUID_VCS_STATE,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ,
		read_volume_state, NULL, &volume_state),
	BT_GATT_CCC(volume_state_cccd_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_VCS_CONTROL,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE,
		NULL, write_volume_control_point, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_VCS_FLAGS,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ,
		read_volume_flags, NULL, &volume_flags),
	BT_GATT_CCC(volume_flags_cccd_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

int main(void)
{
	int err;
	int ret;

	if (!gpio_is_ready_dt(&info_button)) {
		printk("Error: info button device %s is not ready\n",
		       info_button.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&info_button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, info_button.port->name, info_button.pin);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&info_button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, info_button.port->name, info_button.pin);
		return 0;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(info_button.pin));
	gpio_add_callback(info_button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", info_button.port->name, info_button.pin);

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