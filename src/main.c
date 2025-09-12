#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include "VolumeControlService.h"

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

static const struct gpio_dt_spec info_button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static struct k_work adv_start_work;
static struct k_work_delayable status_led_work;
static struct gpio_callback button_cb;

static void status_led_handler(struct k_work *work)
{
	(void)(work);

	gpio_pin_toggle_dt(&status_led);
	k_work_schedule(&status_led_work, K_MSEC(1000));
}

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
	(void)(conn);

	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
	} else {
		printk("Connected\n");
		k_work_cancel_delayable(&status_led_work);
		gpio_pin_set_dt(&status_led, 1); // Turn on LED
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
	k_work_submit(&adv_start_work);
	k_work_schedule(&status_led_work, K_MSEC(1000));
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

	k_work_init(&adv_start_work, adv_start_handler);
	k_work_submit(&adv_start_work);
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("\nVolume State:\n");
	printk("  Volume Setting: %d\n", volume_state.volume_setting);
	printk("  Mute: %d\n", volume_state.mute);
	printk("  Change Counter: %d\n", volume_state.change_counter);

	printk("Volume Flags:\n");
	printk("  Volume_Setting_Persisted: %d\n", (volume_flags & 0x01)); // Get's the first bit (not really needed since it's the only one defined anyway)
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

int8_t info_button_init() {
	int ret;

	if (!gpio_is_ready_dt(&info_button)) {
		printk("Error: info button device %s is not ready\n",
		       info_button.port->name);
		return -1;
	}

	ret = gpio_pin_configure_dt(&info_button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, info_button.port->name, info_button.pin);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&info_button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n", ret, info_button.port->name, info_button.pin);
		return ret;
	}

	gpio_init_callback(&button_cb, button_pressed, BIT(info_button.pin));
	gpio_add_callback(info_button.port, &button_cb);

	return 0;
}

uint8_t init() {
	int err;
	int ret;

	ret = info_button_init();
	if (ret != 0) {
		printk("Failed to initialize info button: %d\n", ret);
		return 0;
	}

	if (!device_is_ready(status_led.port)) {
		printk("Status LED device not ready\n");
		return 0;
	}
	gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_ACTIVE);

	k_work_init_delayable(&status_led_work, status_led_handler);
	k_work_schedule(&status_led_work, K_MSEC(1000));

	err = bt_enable(bt_ready);
	if (err) {
		printk("bt_enable failed (%d)\n", err);
		return 0;
	}

	return 1;
}

int main(void)
{
	if (!init()) {
		printk("Initialization failed. Stopping.\n");
		return -1;
	}

	for (;;) {
		k_sleep(K_SECONDS(1));
	}
}