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

static const struct gpio_dt_spec info_button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static struct k_work adv_start_work;
static struct gpio_callback button_cb_data;

static bool notify_enabled;
extern const struct bt_gatt_service_static vcs_svc;

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

static void volume_state_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("Volume State CCCD changed: %u\n", value);
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

void volume_down(uint8_t *volume) {
	*volume = *volume == 0 ? 0 : *volume - 1;
}

void volume_up(uint8_t *volume) {
	*volume = *volume == 255 ? 255 : *volume + 1;
}

void volume_unmute(void) {
	volume_state.mute = 0;
}

void volume_mute(void) {
	volume_state.mute = 1;
}

void change_counter_increment(struct bt_conn *conn) {
	volume_state.change_counter++;
	if (notify_enabled) bt_gatt_notify(conn, &vcs_svc.attrs[2], &volume_state, sizeof(volume_state));
}

/* GATT write handler for Volume Control Point (0x2B7E) */
static ssize_t write_volume_control_point(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	if (offset != 0 || (len != 2 && len != 3)) {
		printk("Invalid attribute length: %d\n", len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t opcode = ((uint8_t *)buf)[0];
	uint8_t operand = ((uint8_t *)buf)[1];

	if (operand != volume_state.change_counter) {
		return 0x80; // Invalid change_counter
	}

	switch (opcode) {
		case 0x00: // Relative Volume Down
			volume_down(&volume_state.volume_setting);
			break;
		case 0x01: // Relative Volume Up
			volume_up(&volume_state.volume_setting);
			break;
		case 0x02: // Unmute/Relative Volume Down
			volume_unmute();
			volume_down(&volume_state.volume_setting);
			break;
		case 0x03: // Unmute/Relative Volume Up
			volume_unmute();
			volume_up(&volume_state.volume_setting);
			break;
		case 0x04: // Set Absolute Volume
			operand = ((uint8_t *)buf)[2]; // Casting it to uint8_t makes sure it's in 0-255 range
			volume_state.volume_setting = operand;
			break;
		case 0x05: // Unmute
			volume_unmute();
			break;
		case 0x06: // Mute
			volume_mute();
			break;
		default:
			return 0x81; // Invalid opcode
	}

	change_counter_increment(conn);

	return 0; // Success
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
	BT_GATT_CCC(volume_state_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_VCS_CONTROL,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, write_volume_control_point, NULL),
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