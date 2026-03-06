/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* Button alias: sw0 */
#define SW0_NODE DT_ALIAS(sw0)

/* LED aliases */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

/* Get button specification */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

/* Get LED specifications */
static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
    GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};

#define NUM_LEDS ARRAY_SIZE(leds)

/* Button callback structure */
static struct gpio_callback button_cb_data;

/* Current active LED index */
static int current_led = 0;

/* Update LEDs so that only one is ON */
static void update_leds(int active_led)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        gpio_pin_set_dt(&leds[i], (i == active_led) ? 1 : 0);
    }

    printk("Current LED: LED%d\n", active_led);
}

/* Button interrupt callback */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    /* Move to next LED in circular sequence */
    current_led = (current_led + 1) % NUM_LEDS;
    update_leds(current_led);
}

int main(void)
{
    int ret;

    /* Check button device */
    if (!device_is_ready(button.port)) {
        printk("Error: button device is not ready\n");
        return -1;
    }

    /* Check all LED devices */
    for (int i = 0; i < NUM_LEDS; i++) {
        if (!device_is_ready(leds[i].port)) {
            printk("Error: LED%d device is not ready\n", i);
            return -1;
        }
    }

    /* Configure LEDs as output and initially OFF */
    for (int i = 0; i < NUM_LEDS; i++) {
        ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            printk("Error: failed to configure LED%d\n", i);
            return -1;
        }
    }

    /* Turn ON LED0 initially */
    update_leds(current_led);

    /* Configure button as input */
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        printk("Error: failed to configure button\n");
        return -1;
    }

    /* Configure button interrupt */
    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Error: failed to configure button interrupt\n");
        return -1;
    }

    /* Initialize callback */
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    printk("System ready. Press SW0 to cycle LEDs.\n");

    /* Main thread does nothing, wait for interrupts */
    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}