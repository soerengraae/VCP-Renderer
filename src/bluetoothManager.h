#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "volumeControlService.h"

#define BT_DEVICE_NAME_FULL CONFIG_BT_DEVICE_NAME
#define BT_DEVICE_NAME_SHORT "Renderer"

extern struct k_work adv_start_work;
extern const struct bt_gatt_service_static vcsSvc;

/* Advertising payload:
 * - Device Name
 * - Flags
 * - Volume Control Service UUID: 0x1844
 */
extern struct bt_data ad[];

void connected(struct bt_conn *conn, uint8_t err);
void disconnected(struct bt_conn *conn, uint8_t reason);
void adv_start_handler(struct k_work *work);
void bt_ready(int err);
uint8_t initBluetooth(void);

#endif