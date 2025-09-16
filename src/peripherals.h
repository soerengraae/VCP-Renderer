/**
 * @file peripherals.h
 * @brief GPIO peripheral management for nRF52DK board
 *
 * Handles status LED and button interactions for the VCP Renderer demonstration.
 * Provides visual feedback for Bluetooth connection status and user input handling.
 */

#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <zephyr/kernel.h>
#include <zephyr/autoconf.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

/** @brief GPIO specification for info button (typically button 1 on nRF52DK) */
extern struct gpio_dt_spec infoButton;

/** @brief GPIO specification for status LED (typically LED 1 on nRF52DK) */
extern struct gpio_dt_spec statusLed;

/** @brief Delayable work item for LED blinking during advertising */
extern struct k_work_delayable statusLedWork;

/** @brief GPIO callback structure for button press handling */
extern struct gpio_callback buttonCb;

/**
 * @brief Work handler for status LED blinking
 * @param work Work item that triggered this handler
 */
void statusLedHandler(struct k_work *work);

/**
 * @brief GPIO interrupt callback for button presses
 * @param dev GPIO device that triggered the interrupt
 * @param cb GPIO callback structure
 * @param pins Bitmask of pins that triggered the interrupt
 */
void buttonPressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

/**
 * @brief Initialize info button GPIO and interrupt
 * @return 1 on success, 0 on failure
 */
uint8_t initButton(void);

/**
 * @brief Initialize status LED GPIO and work queue
 * @return 1 on success, 0 on failure
 */
uint8_t initStatusLED(void);

#endif