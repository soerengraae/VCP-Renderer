#include "VolumeControlService.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(vcs, 4);

uint8_t volume_flags = 0x00;
static bool notify_state_enabled;
static bool notify_flags_enabled;

struct volume_state_t volume_state = {
	.volume_setting = 128,
	.mute = 0,
	.change_counter = 0,
};

void notify_volume_state(struct bt_conn *conn) {
    if (notify_state_enabled) {
        bt_gatt_notify(conn, &vcs_svc.attrs[2], &volume_state, sizeof(volume_state));
    }
}

/* GATT read handler for Volume State (0x2B7D) */
ssize_t read_volume_state(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &volume_state, sizeof(volume_state));
}

/* GATT read handler for Volume State Flags (0x2B7F) */
ssize_t read_volume_flags(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &volume_flags, sizeof(volume_flags));
}

/* Volume State Characteristic Client Configuration Descriptor (CCCD) changed handler */
void volume_state_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	LOG_DBG("Volume State CCCD changed: %u\n", value);
	notify_state_enabled = (value == BT_GATT_CCC_NOTIFY);
}

/* Volume Flags Characteristic Client Configuration Descriptor (CCCD) changed handler */
void volume_flags_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	LOG_DBG("Volume Flags CCCD changed: %u\n", value);
	notify_flags_enabled = (value == BT_GATT_CCC_NOTIFY);
}

void volume_flags_set(uint8_t flags, struct bt_conn *conn) {
	volume_flags = flags;
	if (notify_flags_enabled) {
		bt_gatt_notify(conn, &vcs_svc.attrs[7], &volume_flags, sizeof(volume_flags));
	}
}

// Opcode implementations
void volume_down(uint8_t *volume) {
	*volume = *volume == VOLUME_MIN ? VOLUME_MIN : *volume - VOLUME_STEP_SIZE; // Fulfills equation requirement Volume_Setting = max(VolumeSetting - Step Size, 0)
}

void volume_up(uint8_t *volume) {
	*volume = *volume == VOLUME_MAX ? VOLUME_MAX : *volume + VOLUME_STEP_SIZE; // Fulfills equation requirement Volume_Setting = min(VolumeSetting + Step Size, 255)
}

void volume_set(uint8_t *volume, uint8_t new_volume) {
	*volume = new_volume; // Fulfills equation requirement Volume_Setting = New Volume
}

void volume_unmute(void) {
	volume_state.mute = 0;
}

void volume_mute(void) {
	volume_state.mute = 1;
}

void change_counter_increment(struct bt_conn *conn) {
	volume_state.change_counter++;
	notify_volume_state(conn);
}

/* GATT write handler for Volume Control Point (0x2B7E) */
ssize_t write_volume_control_point(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	if (offset != 0 || (len != 2 && len != 3)) {
		LOG_WRN("Invalid attribute length: %d\n", len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t opcode = ((uint8_t *)buf)[0];
	uint8_t operand = ((uint8_t *)buf)[1];

	if (operand != volume_state.change_counter) {
		LOG_WRN("Invalid Change Counter: %d (expected %d)\n", operand, volume_state.change_counter);
		return BT_GATT_ERR(ERR_INVALID_CHANGE_COUNTER);
	}

	switch (opcode) {
		case VOLUME_DOWN:
			volume_down(&volume_state.volume_setting);
			break;
		case VOLUME_UP:
			volume_up(&volume_state.volume_setting);
			break;
		case VOLUME_DOWN_UNMUTE:
			volume_unmute();
			volume_down(&volume_state.volume_setting);
			break;
		case VOLUME_UP_UNMUTE:
			volume_unmute();
			volume_up(&volume_state.volume_setting);
			break;
		case VOLUME_SET_ABSOLUTE:
			if (len != 3) {
				LOG_WRN("Invalid attribute length for VOLUME_SET_ABSOLUTE: %d\n", len);
				return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
			}
			operand = ((uint8_t *)buf)[2]; // Casting it to uint8_t makes sure it's in 0-255 range
			volume_set(&volume_state.volume_setting, operand);
			break;
		case VOLUME_UNMUTE:
			volume_unmute();
			break;
		case VOLUME_MUTE:
			volume_mute();
			break;

		default:
			LOG_WRN("Invalid Opcode: %d\n", opcode);
			return BT_GATT_ERR(ERR_INVALID_OPCODE);
	}

	switch (opcode) {
		case VOLUME_DOWN:
		case VOLUME_UP:
		case VOLUME_DOWN_UNMUTE:
		case VOLUME_UP_UNMUTE:
		case VOLUME_SET_ABSOLUTE:
			volume_flags_set((0x01 << 0), conn); // Volume changed - set Volume_Setting_Persisted flag
			break;

		default:
			break;
	}

	change_counter_increment(conn);

	return 0; // Success
}