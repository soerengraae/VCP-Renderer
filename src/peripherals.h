#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <zephyr/kernel.h>
#include <zephyr/autoconf.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

extern struct gpio_dt_spec infoButton;
extern struct gpio_dt_spec statusLed;
extern struct k_work_delayable statusLedWork;
extern struct gpio_callback buttonCb;

void statusLedHandler(struct k_work *work);
void buttonPressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
uint8_t initButton(void);
uint8_t initStatusLED(void);

#endif