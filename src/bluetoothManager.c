#include "bluetoothManager.h"
#include "peripherals.h"

LOG_MODULE_REGISTER(bt_mgr, 4);

struct k_work adv_start_work;

struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_NAME_SHORTENED, BT_DEVICE_NAME_SHORT),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x44, 0x18), /* 0x1844 VCS (little-endian)*/
};

/* GATT: Primary service */
BT_GATT_SERVICE_DEFINE(vcsSvc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_VCS),
	BT_GATT_CHARACTERISTIC(BT_UUID_VCS_STATE,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ,
		readVolumeState, NULL, &vcsState),
	BT_GATT_CCC(volumeStateCccdChanged, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_VCS_CONTROL,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE,
		NULL, writeVolumeControlPoint, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_VCS_FLAGS,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ,
		readVolumeFlags, NULL, &volumeFlags),
	BT_GATT_CCC(volumeFlagsCccdChanged, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

void connected(struct bt_conn *conn, uint8_t err)
{
	(void)(conn);

	if (err) {
		LOG_ERR("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
	} else {
		LOG_DBG("Connected\n");
		k_work_cancel_delayable(&statusLedWork);
		gpio_pin_set_dt(&statusLed, 1); // Turn on LED
	}
}

void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_DBG("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
	k_work_submit(&adv_start_work);
	k_work_schedule(&statusLedWork, K_MSEC(1000));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

void adv_start_handler(struct k_work *work)
{
	(void)(work);

	LOG_DBG("Starting advertisement\n");
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (%d)\n", err);
	} else {
		LOG_DBG("Advertising as connectable peripheral\n");
	}
}

void bt_ready(int err)
{
	if (err) {
		LOG_ERR("Bluetooth init failed (%d)\n", err);
		return;
	}

	LOG_DBG("Bluetooth ready, starting advertising\n");

	k_work_init(&adv_start_work, adv_start_handler);
	k_work_submit(&adv_start_work);
}

uint8_t initBluetooth(void) {
	int err;

	err = bt_enable(bt_ready);
	if (err) {
		LOG_ERR("bt_enable failed (%d)\n", err);
		return false;
	}

	return true;
}