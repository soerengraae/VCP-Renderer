/**
 * @file bluetoothManager.h
 * @brief Bluetooth stack management and connection handling
 *
 * Manages Bluetooth Low Energy advertising, connections, and GATT service
 * registration for the Volume Control Profile Renderer implementation.
 */

#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include "volumeControlService.h"

/** @brief Full device name from configuration */
#define BT_DEVICE_NAME_FULL CONFIG_BT_DEVICE_NAME

/** @brief Shortened device name for advertising */
#define BT_DEVICE_NAME_SHORT "Renderer"

/** @brief Work item for deferred advertising restart */
extern struct k_work adv_start_work;

/** @brief Volume Control Service GATT service definition */
extern const struct bt_gatt_service_static vcsSvc;

/**
 * @brief Bluetooth LE advertising data
 * @details Contains:
 * - Device Name (shortened)
 * - BLE Flags (General Discoverable, BR/EDR not supported)
 * - Volume Control Service UUID: 0x1844
 */
extern struct bt_data ad[];

/**
 * @brief Bluetooth connection established callback
 * @param conn Bluetooth connection handle
 * @param err Connection error code (0 = success)
 */
void connected(struct bt_conn *conn, uint8_t err);

/**
 * @brief Bluetooth connection terminated callback
 * @param conn Bluetooth connection handle
 * @param reason Disconnection reason code
 */
void disconnected(struct bt_conn *conn, uint8_t reason);

/**
 * @brief Work handler to restart advertising
 * @param work Work item that triggered this handler
 */
void adv_start_handler(struct k_work *work);

/**
 * @brief Bluetooth stack ready callback
 * @param err Bluetooth initialization error code (0 = success)
 */
void bt_ready(int err);

/**
 * @brief Initialize Bluetooth stack and start advertising
 * @return 1 on success, 0 on failure
 */
uint8_t initBluetooth(void);

#endif