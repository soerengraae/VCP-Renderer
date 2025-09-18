/**
 * @file volumeControlService.h
 * @brief Bluetooth Volume Control Service (VCS) implementation
 *
 * This module implements the Bluetooth Volume Control Profile (VCP) Renderer role,
 * providing a GATT service that allows remote clients to control volume settings.
 * Complies with Bluetooth VCP specification for volume state management and control.
 */

#ifndef VOLUMECONTROLSERVICE_H
#define VOLUMECONTROLSERVICE_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

/** @brief Volume adjustment step size for relative volume changes */
#define VOLUME_STEP_SIZE 10

/** @brief Maximum volume level (0-255 range per VCP spec) */
#define VOLUME_MAX 255

/** @brief Minimum volume level */
#define VOLUME_MIN 0

/** @brief Volume Control Service GATT service definition */
extern const struct bt_gatt_service_static vcsSvc;

/**
 * @brief VCP-specific error codes for Volume Control Point operations
 *
 * These error codes are returned when Volume Control Point writes fail
 * due to protocol violations or invalid parameters.
 */
enum ERROR_CODES {
  ERR_INVALID_CHANGE_COUNTER = 0x80,  /**< Change counter mismatch */
  ERR_INVALID_OPCODE = 0x81,          /**< Unsupported or invalid opcode */
};

/**
 * @brief Volume Control Point opcodes per VCP specification
 *
 * These opcodes define the supported volume control operations that can
 * be written to the Volume Control Point characteristic (0x2B7E).
 */
enum OPCODES {
  VOLUME_DOWN,          /**< Decrease volume by step size */
  VOLUME_UP,            /**< Increase volume by step size */
  VOLUME_DOWN_UNMUTE,   /**< Decrease volume and unmute */
  VOLUME_UP_UNMUTE,     /**< Increase volume and unmute */
  VOLUME_SET_ABSOLUTE,  /**< Set absolute volume level */
  VOLUME_UNMUTE,        /**< Unmute without changing volume */
  VOLUME_MUTE           /**< Mute without changing volume */
};

/**
 * @brief Volume state structure matching VCP Volume State characteristic (0x2B7D)
 *
 * This structure represents the current volume state that gets exposed via
 * the Volume State characteristic and sent in notifications when changes occur.
 */
struct volumeState {
	uint8_t volumeSetting;  /**< Current volume level (0-255) */
	uint8_t mute;          /**< Mute state: 0=unmuted, 1=muted */
	uint8_t changeCounter; /**< Synchronization counter for client coordination */
};

/** @brief Volume flags characteristic value (0x2B7F) */
extern uint8_t volumeFlags;

/** @brief Current volume state exposed via Volume State characteristic */
extern struct volumeState vcsState;

/**
 * @brief Send volume state notification to connected client
 * @param conn Bluetooth connection handle
 */
void notifyVolumeState(struct bt_conn *conn);

/**
 * @brief GATT read handler for Volume State characteristic (0x2B7D)
 * @param conn Bluetooth connection handle
 * @param attr GATT attribute being read
 * @param buf Output buffer for read data
 * @param len Maximum bytes to read
 * @param offset Read offset into characteristic value
 * @return Number of bytes read on success, negative error code on failure
 */
ssize_t readVolumeState(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);

/**
 * @brief GATT read handler for Volume Flags characteristic (0x2B7F)
 * @param conn Bluetooth connection handle
 * @param attr GATT attribute being read
 * @param buf Output buffer for read data
 * @param len Maximum bytes to read
 * @param offset Read offset into characteristic value
 * @return Number of bytes read on success, negative error code on failure
 */
ssize_t readVolumeFlags(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);

/**
 * @brief Volume State CCCD change handler
 * @param attr GATT attribute (CCCD) that changed
 * @param value New CCCD value (notifications enabled/disabled)
 */
void volumeStateCccdChanged(const struct bt_gatt_attr *attr, uint16_t value);

/**
 * @brief Volume Flags CCCD change handler
 * @param attr GATT attribute (CCCD) that changed
 * @param value New CCCD value (notifications enabled/disabled)
 */
void volumeFlagsCccdChanged(const struct bt_gatt_attr *attr, uint16_t value);

/**
 * @brief Update volume flags and notify clients if enabled
 * @param flags New volume flags value
 * @param conn Bluetooth connection handle for notifications
 */
void volumeFlagsSet(uint8_t flags, struct bt_conn *conn);

/**
 * @brief GATT write handler for Volume Control Point characteristic (0x2B7E)
 * @details Processes VCP opcodes according to Bluetooth VCP specification.
 *          Validates change counter for operation synchronization.
 * @param conn Bluetooth connection handle
 * @param attr GATT attribute being written
 * @param buf Input buffer containing opcode and parameters
 * @param len Length of input buffer (2-3 bytes expected)
 * @param offset Write offset (must be 0)
 * @param flags GATT write flags
 * @return Number of bytes processed on success, negative error code on failure
 */
ssize_t writeVolumeControlPoint(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

/**
 * @brief Decrease volume by step size with bounds checking
 * @param volume Pointer to current volume level to modify
 */
void volumeDown(uint8_t *volume);

/**
 * @brief Increase volume by step size with bounds checking
 * @param volume Pointer to current volume level to modify
 */
void volumeUp(uint8_t *volume);

/**
 * @brief Set mute state to muted
 */
void volumeMute(void);

/**
 * @brief Set mute state to unmuted
 */
void volumeUnmute(void);

/**
 * @brief Increment change counter and notify volume state change
 * @param conn Bluetooth connection handle for notifications
 */
void changeCounterIncrement(struct bt_conn *conn);

#endif