#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/kfifo.h>
#include "cdata_ioctl.h"

#define BUF_SIZE 64
#define CDATA_FB_MINOR 58
#define LCD_SIZE        (320*240*4)

unsigned char *fbmem;

struct cdata_t {
    unsigned char *buf;
    int	idx;
    wait_queue_head_t   wq;
    struct semaphore    sem;
    struct timer_list   flush_timer;
    spinlock_t		lock;

    struct work_struct      work;
    struct kfifo *cdata_fifo;
    spinlock_t fifo_lock;

};

static void flush_buffer(struct work_struct *work)
{
	struct cdata_t *cdata = container_of(work, struct cdata_t, work); 
	int i, j;
	wait_queue_head_t       *wq;
	int index;
	unsigned char *pixel;
	int len;

	pixel = kmalloc(BUF_SIZE, GFP_KERNEL);

	down_interruptible(&cdata->sem);
	wq = &cdata->wq;
	index = cdata->idx;

	len = kfifo_len(cdata->cdata_fifo);
	printk("Prepare to flush, kfifo length = %d\n", len);
	if(kfifo_get(cdata->cdata_fifo, pixel, len) != len) {
		printk("Simon: kfifo_get ERROR\n");
	}
	printk("Finish Flush Buffer!!\n");
	up(&cdata->sem);

	kfree(pixel);
#if 0
	for(i=0; i < BUF_SIZE; i++)
	{
		//writel(0x000000ff, fbmem);
		//fbmem = fbmem + 4;

		//Simon: ARM9 is little endian SOC
		//Data is filled form right side byte
		//writeb(*priv++, fbmem++);
		//writeb(*priv++, fbmem++);
		//writeb(*priv++, fbmem++);
		//writeb(*priv++, fbmem++);

		writeb(pixel[i], fbmem);
		fbmem++;
		//for (j = 0; j < 10000000; j++);
		//schedule();	
	}
/*
        for (i = 0; i < index; i++) {
            writeb(pixel[i], fbmem+offset);
            offset++;
            if (offset >= LCD_SIZE)
                offset = 0;
            // Lab
            for (j = 0; j < 100000; j++);
        }
*/
	cdata->idx = 0;
	cdata->offset = offset;
#endif

	wake_up(wq);
}

static int cdata_open(struct inode *inode, struct file *filp)
{
	struct cdata_t *cdata;

	printk(KERN_ALERT "CDATA: in open\n");
	cdata= kmalloc(sizeof(struct cdata_t), GFP_KERNEL);
        cdata->buf = kmalloc(BUF_SIZE, GFP_KERNEL);

	spin_lock_init(&cdata->fifo_lock);
	cdata->cdata_fifo = kfifo_alloc(BUF_SIZE, GFP_KERNEL,
					 &cdata->fifo_lock);
	if (IS_ERR(cdata->cdata_fifo)) {
		printk(KERN_ERR "cdata-fb: kfifo_alloc failed\n");
		return PTR_ERR(cdata->cdata_fifo);
	}
	cdata->idx = 0;

	filp->private_data = (void *)cdata;

	init_waitqueue_head(&cdata->wq);

	sema_init(&cdata->sem, 1);

	init_timer(&cdata->flush_timer);

	INIT_WORK(&cdata->work, flush_buffer);

	return 0;
}

static ssize_t cdata_write(struct file *filp, const char *buf, size_t size,
                        loff_t *off)
{
        struct cdata_t *cdata = (struct cdata_t *)filp->private_data;
	int i;
	int idx;
	wait_queue_head_t *wq;
	DECLARE_WAITQUEUE(wait, current);
	struct timer_list *timer;
	unsigned char temp[4];

	down_interruptible(&cdata->sem);

	wq = &cdata->wq;
	timer = &cdata->flush_timer;

	up(&cdata->sem);

	for(i = 0; i < size; i++)
	{
		if(kfifo_len(cdata->cdata_fifo) >= BUF_SIZE)
		{
			printk(KERN_ALERT "cdata_write: buffer is full\n");

			timer->expires = jiffies + 5*HZ;
			timer->function = flush_buffer;
			timer->data =(unsigned long *)&cdata->work; 
			add_timer(timer);

			spin_lock(&cdata->lock);
			add_wait_queue(wq, &wait);
repeat:
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock(&cdata->lock);

			printk("kfifo length = %d\n", kfifo_len(cdata->cdata_fifo));
			if (kfifo_len(cdata->cdata_fifo) >= BUF_SIZE) {
				//spin_unlock_irq(&mse->lock);
				schedule();
				spin_lock(&cdata->lock);
				//spin_lock_irq(&mse->lock);
				goto repeat;
			}

			current->state = TASK_RUNNING;
			remove_wait_queue(&cdata->wq, &wait);
		} else {
			copy_from_user(&temp[0], &buf[i], 1);
			if(!kfifo_put(cdata->cdata_fifo, &temp[0], 1)) {
				printk("Simon: kfifo_put ERROR\n");
			}
		}
	}
	
	return 0;
}

static int cdata_ioctl(struct inode *inode, struct file *filp,
                unsigned int cmd, unsigned long arg)
{
        struct cdata_t *cdata = (struct cdata_t *)filp->private_data;
        int i;

        switch (cmd) {
            case IOCTL_SYNC:
                printk(KERN_ALERT "IOCTL_SYNC.\n");
                return 0;

            case IOCTL_EMPTY:
                printk(KERN_ALERT "IOCTL_EMPTY.\n");
                return 0;
 
        }

        return -ENOTTY;
}

static int cdata_close(struct inode *inode, struct file *filp)
{
        struct cdata_t *cdata = (struct cdata_t *)filp->private_data;

        kfree(cdata->buf);
        kfree(cdata);

	del_timer(&cdata->flush_timer);

        return 0;
}

static int cdata_mmap(struct file *filp,
			struct vm_area_struct *vma)
{
	unsigned long from;
	unsigned long to;
	unsigned long size;

	from = vma->vm_start;
	to = 0x33f00000;
	size = vma->vm_end-vma->vm_start;
	
	while(size){
		//remap_page_range(from, to, PAGE_SIZE, PAGE_SHARED);
		
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;	
	}
	
	printk(KERN_ALERT "vma start: %p\n", vma->vm_start);
	printk(KERN_ALERT "vma end: %p\n", vma->vm_end);

	return 0;
}

static struct file_operations cdata_fops = {	
	open:		cdata_open,
	ioctl:          cdata_ioctl,
	write:          cdata_write,
	mmap:		cdata_mmap,
	release:        cdata_close,
};

static struct miscdevice cdata_fb_misc = {
	minor:		CDATA_FB_MINOR,
	name:		"cdata-fb",
	fops:		&cdata_fops,
};

#if 1
static ssize_t cdata_show_version(struct class *cls, char *buf)
{
	return sprintf(buf, "CDATA CLASS V1.0\n");
}

static ssize_t cdata_handle_connect(struct class *cls, const char *buf,
					size_t count)
{
	struct cdata_t *data;
	char str[1];
	int v;
	int i;

	memcpy(str, buf, 1);

	printk(KERN_ALERT "what we write is %c\n", str[0]);
	/* atoi */
	v = str[0];
	v = v - '0';
	
	if ((v != 0) || (v != 1))
	    return 0;

	printk(KERN_ALERT "cdata_dev: connect_enable = %d\n", v);
	return 0;
}

static CLASS_ATTR(cdata, 0666, cdata_show_version, cdata_handle_connect);
static struct class *cdata_class;
#endif

int cdata_init_module(void)
{
	if (misc_register(&cdata_fb_misc) < 0) {
		printk(KERN_INFO "CDATA_FB: can't register driver\n");
        	return -1;
	}
	
	printk(KERN_ALERT "CDATA_FB: register driver DONE\n");

	cdata_class = class_create(THIS_MODULE, "cdata_android");
	class_create_file(cdata_class, &class_attr_cdata);

	return 0;
}

void cdata_cleanup_module(void)
{
	misc_deregister(&cdata_fb_misc);

	class_remove_file(cdata_class, &class_attr_cdata);
	class_destroy(cdata_class);

}

module_init(cdata_init_module);
module_exit(cdata_cleanup_module);

MODULE_LICENSE("GPL");
