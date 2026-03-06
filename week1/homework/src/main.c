/*
 * Multi-threaded LED chase using semaphores in Zephyr RTOS
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* LED aliases from device tree */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

/* Get LED specifications */
static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
    GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};

#define NUM_LEDS ARRAY_SIZE(leds)

/* Stack size and thread priority */
#define STACK_SIZE 512
#define THREAD_PRIORITY 5

/* Delay between LED changes */
#define LED_DELAY_MS 1

/* Semaphores: one per LED thread */
K_SEM_DEFINE(sem_led0, 1, 1);  /* Start with LED0 enabled */
K_SEM_DEFINE(sem_led1, 0, 1);
K_SEM_DEFINE(sem_led2, 0, 1);
K_SEM_DEFINE(sem_led3, 0, 1);

static struct k_sem *led_sems[NUM_LEDS] = {
    &sem_led0,
    &sem_led1,
    &sem_led2,
    &sem_led3
};

/* Thread stacks */
K_THREAD_STACK_DEFINE(stack0, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack1, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack2, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack3, STACK_SIZE);

/* Thread control blocks */
static struct k_thread thread_data0;
static struct k_thread thread_data1;
static struct k_thread thread_data2;
static struct k_thread thread_data3;

/* Turn ON only one LED */
static void update_leds(int active_led)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        gpio_pin_set_dt(&leds[i], (i == active_led) ? 1 : 0);
    }
    printk("LED%d ON\n", active_led);
}

/* Generic LED thread function */
void led_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int led_id = (int)(intptr_t)p1;
    int next_led = (led_id + 1) % NUM_LEDS;

    while (1) {
        /* Wait until this LED gets the baton */
        k_sem_take(led_sems[led_id], K_FOREVER);

        /* Light this LED only */
        update_leds(led_id);

        /* Keep it on for a while */
        k_sleep(K_MSEC(LED_DELAY_MS));

        /* Pass baton to next LED thread */
        k_sem_give(led_sems[next_led]);
    }
}

int main(void)
{
    int ret;

    printk("Starting semaphore-based LED chase...\n");

    /* Check LED devices and configure them */
    for (int i = 0; i < NUM_LEDS; i++) {
        if (!device_is_ready(leds[i].port)) {
            printk("Error: LED%d device not ready\n", i);
            return -1;
        }

        ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            printk("Error: failed to configure LED%d\n", i);
            return -1;
        }
    }

    /* Create 4 LED threads */
    k_thread_create(&thread_data0, stack0, STACK_SIZE,
                    led_thread, (void *)(intptr_t)0, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&thread_data1, stack1, STACK_SIZE,
                    led_thread, (void *)(intptr_t)1, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&thread_data2, stack2, STACK_SIZE,
                    led_thread, (void *)(intptr_t)2, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&thread_data3, stack3, STACK_SIZE,
                    led_thread, (void *)(intptr_t)3, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    /* Main thread can sleep forever */
    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}