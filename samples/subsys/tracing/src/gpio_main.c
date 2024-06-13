#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>


/* size of stack area used by each thread */
#define STACKSIZE (2048)

/* scheduling priority used by each thread */
#define PRIORITY 7


void test_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	printk("Interrupt detected!\n");
}


int gpio_sample(void) 
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    /* Configure pin 0 as output and toggle */
	gpio_pin_configure(dev, 0, GPIO_OUTPUT);
    gpio_pin_set(dev, 0, 1);
    gpio_pin_set(dev, 0, 0);

    /* Configure pin 1 as input */
    gpio_pin_configure(dev, 1, GPIO_INPUT);

    /* Setup pin 1 for interrupt */
    gpio_pin_interrupt_configure(dev, 1, GPIO_INT_LEVEL_HIGH);
    static struct gpio_callback gpio_cb;
	gpio_init_callback(&gpio_cb, test_handler, 1);
	gpio_add_callback(dev, &gpio_cb);

    /* Trigger interrupt */
	gpio_pin_set(dev, 0, 1);

    /* Remove interrupt */
	gpio_remove_callback(dev, &gpio_cb);
}


void threadA(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	gpio_sample();
}


K_THREAD_DEFINE(thread_a, STACKSIZE, threadA, NULL, NULL, NULL,
		PRIORITY, 0, 0);
        