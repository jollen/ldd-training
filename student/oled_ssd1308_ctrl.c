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
 *  History:     ysh  7-25-2016          Create
 *************************************************************/


#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include "oled_ssd1308_spi.h"

#undef pr_fmt
#define pr_fmt(fmt) "%s:"fmt

enum DataCmd_e {
	eCommand,
	eData
};

static int  _spi_tx(u8 byte);
static int  _spi_tx_batch(u8 *pdata, ssize_t len);
static void _set_dataMode(void);
static void _set_cmdMode(void);
static void _set_dc_pin(enum DataCmd_e data_cmd);
static void _write_cmd(u8 cmd);
static void _write_data_batch(u8 *pData, ssize_t len);
static void _set_page_addr(u8 page);
static void _set_lower_addr(u8 low);
static void _set_high_addr(u8 high);
static void _raise_reset(void);
static void _release_reset(void);

static struct oled_platform_data_t *pOLED;


static int _spi_tx(u8 byte)
{
	int ret;
	
	if (IS_ERR(pOLED->spi))
		return PTR_ERR(pOLED->spi);
	
	ret = spi_write(pOLED->spi, &byte, sizeof(u8));
	if (ret)
		printk(KERN_WARNING "%s: error\n", __func__);
	
	return ret;
}


static int _spi_tx_batch(u8 *pData, ssize_t len)
{
	int ret;

	if (IS_ERR(pOLED->spi))
		return PTR_ERR(pOLED->spi);
	
	ret = spi_write(pOLED->spi, pData, len);
	if (ret)
		printk(KERN_WARNING "%s: error\n", __func__);

	return ret;
}

			 
static void _set_dataMode(void)
{
	gpio_set_value(pOLED->ad_pin, 1);
}


static void _set_cmdMode(void)
{
	gpio_set_value(pOLED->ad_pin, 0);
}


static void _set_dc_pin(enum DataCmd_e data_cmd)
{
	if(data_cmd == eCommand)
		_set_cmdMode();
	else /* data_cmd == eData */
		_set_dataMode();
}


static void _write_cmd(u8 cmd)
{
	_set_dc_pin(eCommand);
	_spi_tx(cmd);
}


void _write_data_batch(u8 *pData, ssize_t len)
{
	_set_dc_pin(eData);
	_spi_tx_batch(pData, len);
}

void oled_on()
{
	_write_cmd(0xafu);
}


void oled_off()
{
	_write_cmd(0xaeu);
}


static void _set_page_addr(u8 page)
{
	_write_cmd((page & 0x0fu) + 0xb0u);
}


static void _set_lower_addr(u8 low)
{
	_write_cmd(low & 0x0fu);
}


static void _set_high_addr(u8 high)
{
	_write_cmd((high & 0x0fu) + 0x10u);
}

static void _raise_reset()
{
	gpio_set_value(pOLED->reset_pin, 0);
}


static void _release_reset()
{
	gpio_set_value(pOLED->reset_pin, 1);
}


void oled_reset()
{
	_raise_reset();
	mdelay(1);
	_release_reset();


	/* Sleep mode : AEh */
	_write_cmd(0xaeu);		/* display off */

	_write_cmd(0xd5u);		/* Set Display clocDivide Ratio/Oscillator Frequency */
	_write_cmd(0x80u);		/* 105Hz */

	_write_cmd(0xa8u);		/* Set Multiplex ratio */
	_write_cmd(0x3fu);		/* Set 64 mux */

	_write_cmd(0xd9u);		/* Set Pre-charge Period */
	_write_cmd(0x22u);		/* Default value */

	_write_cmd(0x20u);		/* Set Memory Addressing Mode */
	_write_cmd(0x00u);		/* Default value */
	
	_write_cmd(0xa0u);		/* 0xa0: seg re-map 0-->127; 0xa1: seg re-map 127-->0 */
	
	_write_cmd(0xc0u);		/* 0xc0: com scan direction COM0-->COM(N-1); 0xc8: com scan direction COM(N-1)-->COM0 */
	
	_write_cmd(0xdau);		/* Set Com Pins Hardware Configuration */
	_write_cmd(0x12u);		/* Default value */
	
	_write_cmd(0xadu);		/* External or Internal Iref Selection */
	_write_cmd(0x00u);		/* 0x00: External Iref ; 0x10: Internal Iref*/
	
	_write_cmd(0x81u);		/* Set Contrast Control */
	_write_cmd(0x50u);		/* 1..256£¬adjust in application */
	
	_write_cmd(0xb0u);		/* Set Page start Address for Page Addressing Monde */
	
	_write_cmd(0xd3u);		/* Set Display Offset */
	_write_cmd(0x00u);		/* offset=0, The value is default */
	
	_write_cmd(0x21u);		/* Set Column Address */
	_write_cmd(0x00u);		/* Column Start Address */
	_write_cmd(0x7fu);		/* Column End Address */
	
	_write_cmd(0x22u);		/* Set Page Address */
	_write_cmd(0x00u);		/* Page Start Address */
	_write_cmd(0x07u);		/* Page End Address */
	
	_write_cmd(0x10u);		/* Set Higher Column Start Address for Page Addressing Mode */
	_write_cmd(0u);                 /* Set Lower Column Start Address for Page Addressing Mode */
	
	_write_cmd(0x40u);		/* Set Display Start Line */
	
	_write_cmd(0xa6u);		/* 0xa6: Display Normal; 0xa7: Display Inverse */
	
	_write_cmd(0xa4u);		/* 0xa4: Resume to RAM content display; 0xa5: Display 0xa5, ignores RAM content */
	
	_write_cmd(0xdbu);		/* Set VCOMH Level */
	_write_cmd(0x30u);		/* 0x00: 0.65Vcc; 0x20: 0.77Vcc; 0x30: 0.83Vcc */	
	
	oled_on();
	printk(KERN_INFO "%s: complete\n", __func__);
}


void oled_flush()
{
	ssize_t
		page,
		page_nr,
		pixel_x;
	u8 *fb;
	int i;

	pr_debug("enter\n", __func__);
	
	page_nr = pOLED->page_nr;
	pixel_x = pOLED->pixel_x;

	if (pOLED->reverse_pixel) {
		for (i=0; i<pOLED->fb_size; i++)
			pOLED->fb_reverse[i] = ~pOLED->fb[i];
		fb = pOLED->fb_reverse;
	}
	else
		fb = pOLED->fb;

	if (IS_ERR(fb)) {
		printk(KERN_WARNING "%s: fb error\n", __func__);
		return;
	}

	for(page=0; page<page_nr; page++)
	{
		_set_page_addr(page);
		_set_lower_addr(0);
		_set_high_addr(0);
		_write_data_batch(fb + page * pixel_x, pixel_x);
	}

	pr_debug("exit\n", __func__);
}


void oled_paint(u8 byte)
{
	ssize_t
		page,
		page_offset,
		pixel,
		page_nr,
		pixel_x;
	u8 *fb;

	// down_interruptible(&pOLED->sem);
	pr_debug("enter\n", __func__);
	
	page_nr = pOLED->page_nr;
	pixel_x = pOLED->pixel_x;
	fb = pOLED->fb;

	if (IS_ERR(fb)) {
		printk(KERN_WARNING "%s: fb error\n", __func__);
		return;
	}
	
	for(page=0; page<page_nr; page++) {
		page_offset = page * pixel_x;
		pr_debug("page_offset: %d\n", __func__, page_offset);
		for(pixel=0; pixel<pixel_x; pixel++)
			fb[page_offset + pixel] = byte;
	}
	pr_debug("data copying done\n", __func__);
	
	// up(&pOLED->sem);
	pr_debug("exit\n", __func__);
}


void oled_init(struct oled_platform_data_t *oled)
{
	pr_debug("enter\n", __func__);
	pOLED = oled;
	sema_init(&pOLED->sem, 1);
	oled_reset();
	pr_debug("exit\n", __func__);
}
