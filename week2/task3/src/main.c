/* main.c - Lab Activity 2 - Task 3 Template
 *
 * Task 3:
 *   Compare how sampling is triggered:
 *     (A) Polling: while-loop + k_sleep
 *     (B) Event-driven: k_timer -> k_work -> sampling in thread context
 *
 * NOTE:
 *   Do NOT call adc_read() inside k_timer handler.
 *   Timer handler should only submit a k_work item.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

/* 0 = Polling, 1 = Timer + Work */
#define USE_TIMER_EVENT_DRIVEN  0
#define SAMPLE_PERIOD_MS        500

/* LED (Task 2 optional – keep compatible with Task 1/2) */
#define LED0_NODE DT_ALIAS(led0)
#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define TEMP_THRESHOLD_C 30.0f

/* ADC channel */
static const struct adc_dt_spec adc_channel =
	ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static int16_t adc_buf;
static bool adc_setup_done;

/* ===== Reuse the same sampling+processing function from Task 1/2 ===== */
static int read_and_print_temperature(float *out_temp_c)
{
	int err;

	if (!adc_is_ready_dt(&adc_channel)) {
		printk("ADC %s is not ready\n", adc_channel.dev->name);
		return -EIO;
	}

	if (!adc_setup_done) {
		err = adc_channel_setup_dt(&adc_channel);
		if (err < 0) {
			printk("ADC channel setup failed (err=%d)\n", err);
			return err;
		}
		adc_setup_done = true;
	}

	struct adc_sequence sequence = {
		.buffer = &adc_buf,
		.buffer_size = sizeof(adc_buf),
	};

	err = adc_sequence_init_dt(&adc_channel, &sequence);
	if (err < 0) {
		printk("ADC sequence init failed (err=%d)\n", err);
		return err;
	}

	err = adc_read(adc_channel.dev, &sequence);
	if (err < 0) {
		printk("ADC read failed (err=%d)\n", err);
		return err;
	}

	const int16_t raw = adc_buf;

	/* raw -> mV */
	int32_t mv = raw;
	err = adc_raw_to_millivolts_dt(&adc_channel, &mv);
	if (err < 0) {
		printk("ADC raw to mV conversion failed (err=%d)\n", err);
		return err;
	}

	/* mV -> temperature (LM335: 10 mV/K) */
	float temp_c = ((float)mv / 10.0f) - 273.15f;

	/* Optional LED logic */
	bool led_on = (temp_c > TEMP_THRESHOLD_C);
	gpio_pin_set_dt(&led0, led_on ? 1 : 0);

	printk("raw=%d, mv=%d, temp=%.2f C, LED=%s\n",
	       raw, mv, (double)temp_c, led_on ? "ON" : "OFF");

	if (out_temp_c) {
		*out_temp_c = temp_c;
	}
	return 0;
}

/* ===== Task 3 (Event-driven) skeleton ===== */
#if USE_TIMER_EVENT_DRIVEN
static struct k_timer sample_timer;
static struct k_work  sample_work;

/* Runs in workqueue context (safe place to call adc_read) */
static void sample_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	read_and_print_temperature(NULL);
}

/* Runs in timer context (do NOT touch ADC here) */
static void sample_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	k_work_submit(&sample_work);
}
#endif

int main(void)
{
	int err;

	printk("Lab Activity 2 - Task 3 Template\n");

	/* LED init (optional) */
	if (!gpio_is_ready_dt(&led0)) {
		printk("LED device not ready\n");
		return 0;
	}
	err = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (err < 0) {
		printk("LED configure failed (err=%d)\n", err);
		return 0;
	}

#if USE_TIMER_EVENT_DRIVEN
	k_work_init(&sample_work, sample_work_handler);
	k_timer_init(&sample_timer, sample_timer_handler, NULL);
	k_timer_start(&sample_timer, K_NO_WAIT, K_MSEC(SAMPLE_PERIOD_MS));

	printk("Mode = TIMER (event-driven)\n");

	while (1) {
		k_sleep(K_FOREVER);
	}
#else
	printk("Mode = POLLING\n");

	while (1) {
		read_and_print_temperature(NULL);
		k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
	}
#endif
}