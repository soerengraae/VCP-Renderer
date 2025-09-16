/**
 * @file main.c
 * @brief VCP Renderer main application entry point
 *
 * Initializes all subsystems for the Volume Control Profile Renderer demonstration
 * and starts the Bluetooth advertising process. The application runs event-driven
 * with no main loop to conserve power.
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>

#include "volumeControlService.h"
#include "bluetoothManager.h"
#include "peripherals.h"

LOG_MODULE_REGISTER(main, 4);

/**
 * @brief Initialize all application subsystems
 * @details Initializes peripherals (LED, button) and Bluetooth stack in sequence.
 *          Critical failures in LED or Bluetooth will cause initialization to fail.
 *          Button failure is non-critical and only generates a warning.
 * @return 1 on success, 0 on failure
 */
uint8_t init() {
	// Initialize peripherals
	if (!initStatusLED()) {
		LOG_ERR("Status LED initialization failed\n");
		return 0;
	}

	if (!initButton()) {
		LOG_WRN("Info button initialization failed\n");
	}

	// Initialize Bluetooth
	if (!initBluetooth()) {
		LOG_ERR("Bluetooth initialization failed\n");
		return 0;
	}

	return 1;
}

/**
 * @brief Main application entry point
 * @details Initializes all subsystems and starts the VCP Renderer service.
 *          The application is event-driven and does not run a main loop to save power.
 *          All functionality is handled through Bluetooth callbacks and work queues.
 * @return 0 on successful initialization, -1 on failure
 */
int main(void)
{
	if (!init()) {
		LOG_ERR("Initialization failed. Stopping.\n");
		return -1;
	}

	return 0;
}