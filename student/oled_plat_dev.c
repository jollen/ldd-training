
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include "oled_ssd1308_spi.h"


struct oled_platform_data_t oled_platform_data = {
	.spi = NULL,
	.pixel_x = 128,
	.pixel_y = 64,
	.page_nr = 8,
	.reset_pin = 114,
	.ad_pin = 55,
	.led1_pin = 48,
	.led2_pin = 58,
	.fb = NULL
};

struct spi_board_info oled_board_info[] = {
	{
		.modalias = "oled-ssd1308",
		.platform_data = &oled_platform_data,
		.mode = SPI_MODE_0,
		.max_speed_hz = 5000000,
		.bus_num = 1,     /* spidev1.X */
		.chip_select = 0, /* spidevY.0 */
		.mode = 0,
	}
};


static int oled_plat_dev_init(void)
{
	struct spi_master *master;
	struct spi_device *spi;

#if 1
	master = spi_busnum_to_master(oled_board_info[0].bus_num);
	if (!master) {
		printk(KERN_WARNING "%s: spi_busnum_to_master() failed~\n", __func__);
		return -ENODEV;
	}
	
	spi = spi_new_device(master, oled_board_info);
	if (!spi) {
		printk(KERN_WARNING "%s: spi_new_device() failed~\n", __func__);
		return -ENOTTY;
	}

	oled_platform_data.spi = spi;
	

#else /* at booting stage, __init */
	
	spi_register_board_info(oled_board_info, ARRAY_SIZE(oled_board_info));
	
#endif
	printk(KERN_WARNING "%s: complete\n", __func__);
	return 0;
}


static void oled_plat_dev_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);
	spi_unregister_device(oled_platform_data.spi);
}


module_init(oled_plat_dev_init);
module_exit(oled_plat_dev_exit);

MODULE_LICENSE("GPL");
