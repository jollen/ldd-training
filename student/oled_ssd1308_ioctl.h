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
 *  History:     ysh  7-29-2016          Create
 *************************************************************/


#ifndef __OLED_SSD1308_IOCTL_H
#define __OLED_SSD1308_IOCTL_H


#include <linux/ioctl.h>


#define OLED_CLEAR    _IO(0xCE, 1)
#define OLED_RESET    _IO(0xCE, 2)
#define OLED_ON       _IO(0xCE, 3)
#define OLED_OFF      _IO(0xCE, 4)
#define OLED_FEED     _IOW(0xCE, 5, unsigned char)
#define OLED_FLUSH_RATE _IOW(0xCE, 6, unsigned long)


#endif /* __OLED_SSD1308_IOCTL_H */
