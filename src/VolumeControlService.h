#ifndef VOLUMECONTROLSERVICE_H
#define VOLUMECONTROLSERVICE_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#define VOLUME_STEP_SIZE 10
#define VOLUME_MAX 255
#define VOLUME_MIN 0

extern const struct bt_gatt_service_static vcsSvc;

enum ERROR_CODES {
  ERR_INVALID_CHANGE_COUNTER = 0x80,
  ERR_INVALID_OPCODE = 0x81,
};

enum OPCODES {
  VOLUME_DOWN,
  VOLUME_UP,
  VOLUME_DOWN_UNMUTE,
  VOLUME_UP_UNMUTE,
  VOLUME_SET_ABSOLUTE,
  VOLUME_UNMUTE,
  VOLUME_MUTE
};

struct volumeState {
	uint8_t volumeSetting;
	uint8_t mute;
	uint8_t changeCounter;
};

extern uint8_t volumeFlags;
extern struct volumeState vcsState;

void notifyVolumeState(struct bt_conn *conn);
bt_gatt_attr_read_func_t readVolumeState(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
bt_gatt_attr_read_func_t readVolumeFlags(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
void volumeStateCccdChanged(const struct bt_gatt_attr *attr, uint16_t value);
void volumeFlagsCccdChanged(const struct bt_gatt_attr *attr, uint16_t value);
void volumeFlagsSet(uint8_t flags, struct bt_conn *conn);
bt_gatt_attr_write_func_t writeVolumeControlPoint(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

void volumeDown(uint8_t *volume);
void volumeUp(uint8_t *volume);
void volumeMute(void);
void volumeUnmute(void);
void changeCounterIncrement(struct bt_conn *conn);

#endif