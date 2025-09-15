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

int main(void)
{
	if (!init()) {
		LOG_ERR("Initialization failed. Stopping.\n");
		return -1;
	}

	for (;;) {
		k_sleep(K_SECONDS(1));
	}
}