#include "peripherals.h"
#include "volumeControlService.h"

LOG_MODULE_REGISTER(peripherals, 4);

struct gpio_dt_spec infoButton = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
struct gpio_dt_spec statusLed = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
struct k_work_delayable statusLedWork;
struct gpio_callback buttonCb;

void statusLedHandler(struct k_work *work)
{
	(void)(work);

	gpio_pin_toggle_dt(&statusLed);
	k_work_schedule(&statusLedWork, K_MSEC(1000));
}

void buttonPressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("\nVolume State:\n");
	LOG_INF("  Volume Setting: %d\n", vcsState.volumeSetting);
	LOG_INF("  Mute: %d\n", vcsState.mute);
	LOG_INF("  Change Counter: %d\n", vcsState.changeCounter);

	LOG_INF("Volume Flags:\n");
	LOG_INF("  Volume_Setting_Persisted: %d\n", (volumeFlags & 0x01)); // Get's the first bit (not really needed since it's the only one defined anyway)
}

uint8_t initButton(void) {
  int ret;

  if (!gpio_is_ready_dt(&infoButton)) {
		LOG_ERR("Error: info button device %s is not ready\n", infoButton.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&infoButton, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d\n", ret, infoButton.port->name, infoButton.pin);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&infoButton, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n", ret, infoButton.port->name, infoButton.pin);
		return 0;
	}

	gpio_init_callback(&buttonCb, buttonPressed, BIT(infoButton.pin));
	gpio_add_callback(infoButton.port, &buttonCb);

	if (ret != 0) {
		LOG_ERR("Failed to initialize info button: %d\n", ret);
		return 0;
	}

	return 1;
}

uint8_t initStatusLED(void) {
  int ret;

  if (!device_is_ready(statusLed.port)) {
		LOG_ERR("Status LED device not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&statusLed, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure status LED pin: %d\n", ret);
		return 0;
	}

	k_work_init_delayable(&statusLedWork, statusLedHandler);
	k_work_schedule(&statusLedWork, K_MSEC(1000));

  return 1;
}