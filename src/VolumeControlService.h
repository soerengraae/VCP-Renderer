#ifndef VOLUMECONTROLSERVICE_H
#define VOLUMECONTROLSERVICE_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>

#define VOLUME_STEP_SIZE 1
#define VOLUME_MAX 255
#define VOLUME_MIN 0

extern const struct bt_gatt_service_static vcs_svc;

enum errorCodes {
  ERR_INVALID_CHANGE_COUNTER = 0x80,
  ERR_INVALID_OPCODE = 0x81,
};

enum opcodes {
  VOLUME_DOWN,
  VOLUME_UP,
  VOLUME_DOWN_UNMUTE,
  VOLUME_UP_UNMUTE,
  VOLUME_SET_ABSOLUTE,
  VOLUME_UNMUTE,
  VOLUME_MUTE
};

struct volume_state {
	uint8_t volume_setting;
	uint8_t mute;
	uint8_t change_counter;
};

extern uint8_t volume_flags;
extern struct volume_state vcs_state;

void notify_volume_state(struct bt_conn *conn);
ssize_t read_volume_state(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
ssize_t read_volume_flags(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
void volume_state_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value);
void volume_flags_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value);
ssize_t write_volume_control_point(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

void volume_down(uint8_t *volume);
void volume_up(uint8_t *volume);
void volume_mute(void);
void volume_unmute(void);
void change_counter_increment(struct bt_conn *conn);

#endif