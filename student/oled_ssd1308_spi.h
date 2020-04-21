/**************************************************************
 *  Author        :  
 *                    ..  ..
 *                   / | / |            ~~
 *                  /  |/  |    /| \ /  ~
 *                 /   | ~ |   /.|  x  ~
 *                / ~      |~ /  l / L
 *
 *  Description :  
 *
 *
 *  History:     ysh  7-17-2016          Create
 *************************************************************/


#ifndef __OLED_SSD1308_SPI_H
#define __OLED_SSD1308_SPI_H


#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/ioctl.h>


struct oled_platform_data_t {
	struct semaphore sem;
	struct spi_device *spi;
	unsigned int pixel_x;
	unsigned int pixel_y;
	unsigned int page_nr;
	unsigned reset_pin;
	unsigned ad_pin;
	unsigned led1_pin;
	unsigned led2_pin;
	unsigned int fb_size;
	u8 *fb;
	long reverse_pixel;
	u8 *fb_reverse;
};


void oled_reset(void);
void oled_on(void);
void oled_off(void);
void oled_paint(u8 byte);
void oled_flush(void);
void oled_init(struct oled_platform_data_t *);


#endif /* __OLED_SSD1308_SPI_H */
