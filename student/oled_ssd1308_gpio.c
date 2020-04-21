/**************************************************************
 *  Author        :  
 *                    ..  ..
 *                   / | / |            ~~
 *                  /  |/  |    /| \ /  ~
 *                 /   | ~ |   /.|  x  ~
 *                / ~      |~ /  l / L
 *
 *  Description :  MISC category driver for test OLED-SSD1308
 *
 *
 *  History:     ysh  7-29-2016          Create
 *************************************************************/


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include "oled_ssd1308_spi.h"


#undef pr_fmt
#define pr_fmt(fmt) "%s:"fmt


struct gpio_t {
	unsigned pin;
	unsigned long flag;
	char *label;
};

static struct gpio_t *oled_gpios = NULL;
static struct oled_platform_data_t *pOLED = NULL;
static ssize_t oled_gpio_nr = 0;

int oled_init_gpios(struct oled_platform_data_t *);
void oled_free_gpios(void);


int oled_init_gpios(struct oled_platform_data_t *oled)
{	
	ssize_t i;
	int err;
	struct gpio_t gpios[] = {
		{ oled->reset_pin, GPIOF_DIR_OUT, "OLED-Reset-Pin" },
		{ oled->ad_pin,    GPIOF_DIR_OUT, "OLED-AD-Pin"    },
		{ oled->led1_pin,  GPIOF_DIR_OUT, "LED1-Pin"       },
		{ oled->led2_pin,  GPIOF_DIR_OUT, "LED2-Pin"       }
	};
	
	pOLED = oled;
	oled_gpio_nr = ARRAY_SIZE(gpios);
	oled_gpios = kzalloc(sizeof gpios, GFP_KERNEL);
	memcpy(oled_gpios, gpios, sizeof gpios);
	
	for (i=0; i<oled_gpio_nr; i++) {
		struct gpio_t *io = &gpios[i];
		err = gpio_request_one(io->pin, io->flag, io->label);
		if (err)
			printk(KERN_WARNING "%s: Request GPIO-%d failed~\n",
			       __func__, io->pin);
	}

	gpio_set_value(oled->led1_pin, 0);
	gpio_set_value(oled->led2_pin, 0);
	pr_debug("\n", __func__);
	return 0;
}


void oled_free_gpios()
{	
	ssize_t i;
	ssize_t gpio_nr = oled_gpio_nr;
	struct gpio_t *gpios = oled_gpios;
	
	gpio_set_value(pOLED->led1_pin, 1);
	gpio_set_value(pOLED->led2_pin, 1);
	
	for (i=0; i<gpio_nr; i++)
		gpio_free(gpios[i].pin);
	
	kfree(oled_gpios);
	pr_debug("\n", __func__);
}
