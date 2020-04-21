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
 *  History:     ysh  7-07-2016          Create
 *************************************************************/


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <asm/page.h>
#include "oled_ssd1308_spi.h"
#include "oled_ssd1308_ioctl.h"

#undef pr_fmt
#define pr_fmt(fmt) "%s: "fmt

#define FLUSH_RATE_DEFAULT 200 /* ms */
//#define __SINGLE_WQ
#define __SHARED_QU
//#define __TIMER

#define PIXEL_X    128
#define PIXEL_Y     64

struct ssd1308_t {
	wait_queue_head_t wait_q;
	struct semaphore sem;
	struct work_struct worker;
	struct delayed_work dworker;
	struct timer_list timer;
	struct tasklet_struct tasklet;
	struct workqueue_struct *wq;
	int del_wq;
	int worker_count;
	int timer_count;
	int tasklet_count;
	unsigned long flush_rate_ms;
	struct oled_platform_data_t *platform_data;
};


struct oled_platform_data_t OLED;
extern int oled_init_gpios(struct oled_platform_data_t *oled);
extern void oled_free_gpios(void);


#ifdef __SINGLE_WQ
static void worker_func(struct work_struct *pWork)
{
	struct ssd1308_t *ssd;
	ssd = container_of(pWork, struct ssd1308_t, worker);

	while (ssd->del_wq != 1) {
		oled_flush();
		msleep(ssd->flush_rate_ms);
		// printk(KERN_WARNING "*fb : 0x%X\n", *ssd->platform_data->fb);
	}
	ssd->del_wq = 2;
	pr_debug("exit, flush_rate: %ldms\n", __func__, ssd->flush_rate_ms);
}

#elif defined(__SHARED_QU)

static void dworker_func(struct delayed_work *pWork)
{
	struct ssd1308_t *ssd;
	ssd = container_of(pWork, struct ssd1308_t, dworker);

	oled_flush();
	if (ssd->del_wq == 1) {
		pr_debug("receive killing message\n", __func__);
		ssd->del_wq = 2;
		return;
	}
	schedule_delayed_work(&ssd->dworker, ssd->flush_rate_ms * HZ / 1000);
}
#endif


#ifdef __TIMER
static void timer_func(unsigned long pSSD)
{
	struct ssd1308_t *ssd = (struct ssd1308_t*)pSSD;
	unsigned long expired = jiffies + HZ / 10;
	ssd->timer_count++;
	oled_flush();
	mod_timer(&ssd->timer, expired);
}
#endif


static ssize_t oled_ssd1308_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct ssd1308_t *ssd;
	
	ssd = (struct ssd1308_t*)filp->private_data;
	if (down_interruptible(&ssd->sem)) {
		pr_debug("interrupted", __func__);
		return 0;
	}

#if 0
	u8 b;
	if (!get_user(b, buf)) {
		printk(KERN_INFO "%s: count %d\n", __func__, count);
		printk(KERN_INFO "%s: buf %s\n", __func__, buf);
		printk(KERN_INFO "%s: user data %d\n", __func__, b);
		oled_paint(b);
		oled_flush();
	}
	
#else
	/*
	 * Transform to hexidecimal
	 */
	#define BUF_SIZE 64
	
	int ret;
	long pixel;
	char str[BUF_SIZE];
	size_t i;
	u8 b;

	if (count > BUF_SIZE - 1) {
		printk(KERN_WARNING "%s: user string is too long\n", __func__);
		goto _WRITE_DONE;
	}

	/*
	 * Accept 0 ~ 9, assemble chars to a string
	 */
	for(i=0; i<count; i++) {
		get_user(b, buf + i);
		printk(KERN_INFO "buf[%d] : %d\n", i, b);
		if (b >= '0' && b <= '9') {
			str[i] = b;
			continue;
		}
		str[i] = '\0';
		break;
	}
	
	ret = kstrtol(str, 0, &pixel);
	if (ret == 0) {
		printk(KERN_INFO "%s: pixel %x\n", __func__, pixel);
		oled_paint((u8)pixel);
		oled_flush();
	}
	else
		printk(KERN_WARNING "%s: kstrtol failed~\n");

#endif
	
_WRITE_DONE:
	up(&ssd->sem);
	return count;
}


static long oled_ssd1308_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
	case OLED_CLEAR:
		oled_paint((u8)0);
		break;
		
	case OLED_RESET:
		oled_reset();
		break;
		
	case OLED_ON:
		oled_on();
		break;
		
	case OLED_OFF:
		oled_off();
		break;
		
	case OLED_FEED:
	{
		u8 feed;
		u8 __user *ptr = (void __user *)arg;
		
		if (get_user(feed, ptr))
			return -EFAULT;
		oled_paint(feed);
	}
	break;

	case OLED_FLUSH_RATE:
	{
		struct ssd1308_t *ssd = (struct ssd1308_t*)filp->private_data;
		pr_debug("New flush rate : %ldms\n", __func__, arg);
		ssd->flush_rate_ms = arg;
	}
	break;
		
	default:
		return -ENOTTY;
	}
	
	pr_debug("cmd-%x\n", __func__, cmd);
	return 0;
}


static int oled_ssd1308_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct ssd1308_t *ssd = (struct ssd1308_t*)filp->private_data;
	struct oled_platform_data_t *oled = ssd->platform_data;
	int ret;
	phys_addr_t phys = virt_to_phys((void*)oled->fb);

	pr_debug("enter\n", __func__);
	pr_debug("oled->fb: %p\n", __func__, oled->fb);
	pr_debug("oled->fb_size: %d\n", __func__, oled->fb_size);
	pr_debug("virt_to_phys((void*)oled->fb) : %p >> PAGE_SHIFT : %p\n",
		 __func__,
		 phys,
		 phys >> PAGE_SHIFT);
	
	pr_debug("vma->vm_end: %lx\n", __func__, vma->vm_end);
	pr_debug("vma->vm_start: %lx\n", __func__, vma->vm_start);
	pr_debug("size: %lu\n", __func__, vma->vm_end - vma->vm_start);
	
	ret = remap_pfn_range(vma,
			      vma->vm_start,
			      phys >> PAGE_SHIFT,
			      vma->vm_end - vma->vm_start,
			      vma->vm_page_prot);
	if (ret < 0) {
		printk(KERN_WARNING "%s: remap_pfn_range failed\n", __func__);
		return -EIO;
	}

	pr_debug("exit\n", __func__);
	return 0;
}


static int oled_ssd1308_open(struct inode *inode, struct file *filp)
{
	struct ssd1308_t *ssd;
	
	pr_debug("enter\n", __func__);
	
	/* Allocate memory to private data, and set memory to zero */
	ssd = kzalloc(sizeof(struct ssd1308_t), GFP_KERNEL);
	ssd->platform_data = &OLED;
	filp->private_data = ssd;

	sema_init(&ssd->sem, 1);

	/* work queue */
	ssd->del_wq = 0;
	ssd->flush_rate_ms = FLUSH_RATE_DEFAULT;


#ifdef __SINGLE_WQ
	ssd->wq = create_singlethread_workqueue("oled-flush");
	INIT_WORK(&ssd->worker, worker_func);
	queue_work(ssd->wq, &ssd->worker);
#endif

#ifdef __SHARED_QU
	INIT_DELAYED_WORK(&ssd->dworker, dworker_func);
	schedule_delayed_work(&ssd->dworker, ssd->flush_rate_ms * HZ / 1000);
	//INIT_WORK(&ssd->worker, worker_func);
	//schedule_work(&ssd->worker);
#endif

#ifdef __TIMER
	/* Init timer */
	init_timer(&ssd->timer);
	ssd->timer.function = timer_func;
	ssd->timer.data = (unsigned long)ssd;
	timeout = HZ / 10;
	ssd->timer.expires = jiffies + timeout;
	add_timer(&ssd->timer);
	oled_paint(0xff);
#endif

	pr_debug("exit\n", __func__);
	return 0;
}

static int oled_ssd1308_close(struct inode *inode, struct file *filp)
{
	struct ssd1308_t *ssd;

	ssd = (struct ssd1308_t*)filp->private_data;
	pr_debug("enter\n", __func__);

#ifdef __TIMER
	del_timer_sync(&ssd->timer);
#endif


#ifdef  __SINGLE_WQ
	ssd->del_wq = 1;
	msleep(ssd->flush_rate_ms);

	while (ssd->del_wq != 2)
		msleep(ssd->flush_rate_ms);
	
	flush_workqueue(ssd->wq);
	destroy_workqueue(ssd->wq);
	
#elif defined(__SHARED_QU)
	ssd->del_wq = 1;
	while (!cancel_delayed_work(&ssd->dworker)) {
		msleep(ssd->flush_rate_ms);
	}
	pr_debug("delayed work has been canceled\n", __func__);
#endif
	
	kfree(filp->private_data);
	pr_debug("exit\n", __func__);
	return 0;
}

static struct file_operations oled_fops = {
	.owner = THIS_MODULE,
	.write = oled_ssd1308_write,
	.unlocked_ioctl = oled_ssd1308_ioctl,
	.mmap = oled_ssd1308_mmap,
	.open = oled_ssd1308_open,
	.release = oled_ssd1308_close
};


static struct miscdevice oled_ssd1308_miscdev = {
	.minor = 197, /* Refer to miscdev.h */
	.name = "oled-ssd1308",
	.fops = &oled_fops
};


static ssize_t reverse_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", OLED.reverse_pixel);
}

static ssize_t reverse_store(struct class *class, struct class_attribute *attr,
			const char *buf, size_t count)
{
	long l;
	int ret;

	ret = kstrtol(buf, 0, &l);
	if (ret != 0 || l > 1 || l < 0)
		return 0;
	
	printk(KERN_INFO "%s: reverse %ld\n", __func__, l);
	OLED.reverse_pixel = l;
	oled_flush();
	return 2;
}

static CLASS_ATTR_RW(reverse);
//static CLASS_ATTR(reverse, 0555, reverse_show, reverse_store);
static struct class *oled_class;

static int oled_ssd1308_probe(struct spi_device *spi)
{
	int ret;
	unsigned int x, y;
	struct oled_platform_data_t *pData;
	ssize_t page_nr, fb;

	if (!spi) {
		printk(KERN_WARNING "%s: spi is null. Device is not accessible\n",
		       __func__);
		return -ENODEV;
	}

	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		printk(KERN_WARNING  "%s: spi_setup() failed~\n", __func__);

	pData = (struct oled_platform_data_t*)spi->dev.platform_data;
	OLED = *pData;
	pr_debug("%dx, %dy, %dreset, %dad, %p-spi\n",
	       __func__,
	       pData->pixel_x, pData->pixel_y,
	       pData->reset_pin, pData->ad_pin, spi);

	OLED.spi = spi;
	pr_debug("spi_new_device(), 0x%lx successful~\n",
	       __func__, (unsigned long)OLED.spi);

	/* Allocate memory */
	x = OLED.pixel_x;
	y = OLED.pixel_y / OLED.page_nr;
	OLED.fb_size = x * y;
	fb = OLED.fb_size;
	for (page_nr = 1; fb > PAGE_SIZE; page_nr++, fb -= PAGE_SIZE);
	OLED.fb = kzalloc(PAGE_SIZE * page_nr, GFP_KERNEL);
	OLED.fb_reverse = kmalloc(PAGE_SIZE * page_nr, GFP_KERNEL);

	oled_init_gpios(&OLED);
	oled_init(&OLED);

	pr_debug("before misc_register", __func__);
	ret = misc_register(&oled_ssd1308_miscdev);
	if (ret >= 0) {
		printk(KERN_INFO "%s: Register OLED miscdev successful!\n", __func__);
		oled_class = class_create(THIS_MODULE, "oled");
		if (IS_ERR(oled_class))
			pr_warn("Unable to create OLED class; errno = %ld\n",
				PTR_ERR(oled_class));
		else
			class_create_file(oled_class, &class_attr_reverse);
	}
	else
		printk(KERN_WARNING "%s: Register OLED miscdev failed~\n", __func__);
	
	return ret;
}

static int oled_ssd1308_remove(struct spi_device *spi)
{
	class_remove_file(oled_class, &class_attr_reverse);
	class_destroy(oled_class);
	oled_off();
	oled_free_gpios();
	kfree(OLED.fb);
	misc_deregister(&oled_ssd1308_miscdev);
	printk(KERN_INFO "%s: Unregister ssd miscdev successful~\n", __func__);
	return 0;
}

static const struct of_device_id oled_ssd1308_dt_ids[] = {
	{ .compatible = "visionox,ssd1308" },
	{}
};

MODULE_DEVICE_TABLE(of, oled_ssd1308_dt_ids);


static struct spi_driver oled_ssd1308_driver = {
	.driver = {
		.name = "oled-ssd1308",
		.owner = THIS_MODULE,
		.of_match_table = oled_ssd1308_dt_ids
	},
	.probe = oled_ssd1308_probe,
	.remove = oled_ssd1308_remove
};


module_spi_driver(oled_ssd1308_driver);
MODULE_LICENSE("GPL");
