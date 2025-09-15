#include "volumeControlService.h"

LOG_MODULE_REGISTER(vcs, 4);

uint8_t volumeFlags = 0x00;
static bool notifyStateEnabled;
static bool notifyFlagsEnabled;

struct volumeState vcsState = {
	.volumeSetting = 128,
	.mute = 0,
	.changeCounter = 0,
};

void notifyVolumeState(struct bt_conn *conn) {
    if (notifyStateEnabled) {
        bt_gatt_notify(conn, &vcsSvc.attrs[2], &vcsState, sizeof(vcsState));
    }
}

/* GATT read handler for Volume State (0x2B7D) */
ssize_t readVolumeState(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &vcsState, sizeof(vcsState));
}

/* GATT read handler for Volume State Flags (0x2B7F) */
ssize_t readVolumeFlags(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &volumeFlags, sizeof(volumeFlags));
}

/* Volume State Characteristic Client Configuration Descriptor (CCCD) changed handler */
void volumeStateCccdChanged(const struct bt_gatt_attr *attr, uint16_t value)
{
	LOG_DBG("Volume State CCCD changed: %u\n", value);
	notifyStateEnabled = (value == BT_GATT_CCC_NOTIFY);
}

/* Volume Flags Characteristic Client Configuration Descriptor (CCCD) changed handler */
void volumeFlagsCccdChanged(const struct bt_gatt_attr *attr, uint16_t value)
{
	LOG_DBG("Volume Flags CCCD changed: %u\n", value);
	notifyFlagsEnabled = (value == BT_GATT_CCC_NOTIFY);
}

void volumeFlagsSet(uint8_t flags, struct bt_conn *conn) {
	volumeFlags = flags;
	if (notifyFlagsEnabled) {
		bt_gatt_notify(conn, &vcsSvc.attrs[7], &volumeFlags, sizeof(volumeFlags));
	}
}

// Opcode implementations
void volumeDown(uint8_t *volume) {
	*volume = *volume == VOLUME_MIN ? VOLUME_MIN : *volume - VOLUME_STEP_SIZE; // Fulfills equation requirement Volume_Setting = max(VolumeSetting - Step Size, 0)
}

void volumeUp(uint8_t *volume) {
	*volume = *volume == VOLUME_MAX ? VOLUME_MAX : *volume + VOLUME_STEP_SIZE; // Fulfills equation requirement Volume_Setting = min(VolumeSetting + Step Size, 255)
}

void volume_set(uint8_t *volume, uint8_t new_volume) {
	*volume = new_volume; // Fulfills equation requirement Volume_Setting = New Volume
}

void volumeUnmute(void) {
	vcsState.mute = 0;
}

void volumeMute(void) {
	vcsState.mute = 1;
}

void changeCounterIncrement(struct bt_conn *conn) {
	vcsState.changeCounter++;
	notifyVolumeState(conn);
}

/* GATT write handler for Volume Control Point (0x2B7E) */
ssize_t writeVolumeControlPoint(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	if (offset != 0 || (len != 2 && len != 3)) {
		LOG_WRN("Invalid attribute length: %d\n", len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t opcode = ((uint8_t *)buf)[0];
	uint8_t operand = ((uint8_t *)buf)[1];

	if (operand != vcsState.changeCounter) {
		LOG_WRN("Invalid Change Counter: %d (expected %d)\n", operand, vcsState.changeCounter);
		return BT_GATT_ERR(ERR_INVALID_CHANGE_COUNTER);
	}

	switch (opcode) {
		case VOLUME_DOWN:
			volumeDown(&vcsState.volumeSetting);
			break;
		case VOLUME_UP:
			volumeUp(&vcsState.volumeSetting);
			break;
		case VOLUME_DOWN_UNMUTE:
			volumeUnmute();
			volumeDown(&vcsState.volumeSetting);
			break;
		case VOLUME_UP_UNMUTE:
			volumeUnmute();
			volumeUp(&vcsState.volumeSetting);
			break;
		case VOLUME_SET_ABSOLUTE:
			if (len != 3) {
				LOG_WRN("Invalid attribute length for VOLUME_SET_ABSOLUTE: %d\n", len);
				return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
			}
			operand = ((uint8_t *)buf)[2]; // Casting it to uint8_t makes sure it's in 0-255 range
			volume_set(&vcsState.volumeSetting, operand);
			break;
		case VOLUME_UNMUTE:
			volumeUnmute();
			break;
		case VOLUME_MUTE:
			volumeMute();
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
			volumeFlagsSet((0x01 << 0), conn); // Volume changed - set Volume_Setting_Persisted flag
			break;

		default:
			break;
	}

	changeCounterIncrement(conn);

	return 0; // Success
}