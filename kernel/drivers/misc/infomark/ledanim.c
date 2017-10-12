//#define DEBUG

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "ledanim.h"

static struct timer_list	anim_timer;
static struct ledanim_t *	anim_current = NULL;
static uint32_t				anim_frame_count;
static uint32_t				anim_total_frames;
static uint32_t				anim_repeat_count;

static struct leddrv_func *	leddrv_func = NULL;

static struct timer_list	backled_timer;
static int					is_backled_timer_started = 0;
static struct leddrv_func *	backled_leddrv_func = NULL;

static int g_priority = LED_DEFAULT;

#if 1 
#include "anim_spin.c"
static struct ledanim_t ledanim_boot = {
	.name = "__boot__",
	.interval = 40,
	.anim_count = 3636,
	.anim = anim_spin,
	.repeat = 0,
};
#else
#include "anim_bootup.c"
static struct ledanim_t ledanim_boot = {
	.name = "__boot__",
	.interval = 40,
	.anim_count = 3636,
	.anim = anim_bootup,
	.repeat = 0,
};
#endif

static void ledanimation_timer_function(unsigned long data);

static int _get_led(int index, unsigned char * value)
{
	if(!leddrv_func)
		return -1;

	if(index < leddrv_func->num_leds){
		return leddrv_func->get_led_brightness(leddrv_func->dev, index, value);
	}

	if(!backled_leddrv_func){
		return -1;
	}

	index -= leddrv_func->num_leds;
	return backled_leddrv_func->get_led_brightness(backled_leddrv_func->dev, index, value);
}

static int _set_led(int index, unsigned char value)
{
	if(!leddrv_func)
		return -1;

	if(index < leddrv_func->num_leds){
		return leddrv_func->set_led_brightness(leddrv_func->dev, index, value);
	}

	if(!backled_leddrv_func){
		return -1;
	}

	index -= leddrv_func->num_leds;
	return backled_leddrv_func->set_led_brightness(backled_leddrv_func->dev, index, value);
}

static int _update_led(void)
{
	if(!leddrv_func)
		return -1;
	return leddrv_func->update(leddrv_func->dev);
}

static void _set_led_all_off(void)
{
	int i;

	if(!leddrv_func)
		return;

	for(i = 0; i < leddrv_func->num_leds ; i++){
		leddrv_func->set_led_brightness(leddrv_func->dev, i, 0);
	}
	leddrv_func->update(leddrv_func->dev);
}

static inline void _draw_led_frame(unsigned char * anim)
{
	int i;
	unsigned char value[64];

	if(!leddrv_func)
		return;

	if(leddrv_func->num_leds < sizeof(value)){
		memset(value, 0, sizeof(value));
		for(i = 0; i < leddrv_func->num_leds ; i++){
			leddrv_func->get_led_brightness(leddrv_func->dev, i, &value[i]);
		}
		if(memcmp(value, anim, leddrv_func->num_leds) == 0){
			return;
		}
	}
	for(i = 0; i < leddrv_func->num_leds ; i++){
		leddrv_func->set_led_brightness(leddrv_func->dev, i, *(anim + i));
	}
	leddrv_func->update(leddrv_func->dev);
}

static inline void start_anim_current(struct ledanim_t * ledanim)
{
	if(!leddrv_func)
		return;

	anim_current = ledanim;
	anim_frame_count = 0;
	anim_total_frames = (uint32_t)(anim_current->anim_count / leddrv_func->num_leds);
	anim_repeat_count = anim_current->repeat;
	ledanimation_timer_function(0);
}

static inline void delete_anim_current(void)
{
	if(anim_current){
		if(strcmp(anim_current->name, "__boot__") == 0){
			anim_current = NULL;
			return;
		}
		kfree(anim_current->anim);
		kfree(anim_current);
		anim_current = NULL;
	}
}

static void backled_timer_function(unsigned long data)
{
	unsigned char value;

	if(backled_leddrv_func == NULL)
		return;

	if(backled_leddrv_func->get_led_brightness(backled_leddrv_func->dev, 0, &value) != 0){
		printk(KERN_ERR "%s: backled get led brightness failed\n", __func__);	
	} else {
		value = value ? 0 : 1;
		backled_leddrv_func->set_led_brightness(backled_leddrv_func->dev, 0, value);
		backled_leddrv_func->set_led_brightness(backled_leddrv_func->dev, 1, value);
		backled_leddrv_func->set_led_brightness(backled_leddrv_func->dev, 2, value);
		backled_leddrv_func->update(backled_leddrv_func->dev);
	}
	mod_timer(&backled_timer, jiffies + msecs_to_jiffies(500));
}

static void start_backled_boot_animation(void)
{
	if(backled_leddrv_func == NULL)
		return;

	backled_leddrv_func->set_led_brightness(backled_leddrv_func->dev, 0, 1);
	backled_leddrv_func->set_led_brightness(backled_leddrv_func->dev, 1, 1);
	backled_leddrv_func->set_led_brightness(backled_leddrv_func->dev, 2, 1);
	backled_leddrv_func->update(backled_leddrv_func->dev);

	init_timer(&backled_timer);
	backled_timer.function = backled_timer_function;
	mod_timer(&backled_timer, jiffies + msecs_to_jiffies(500));
	is_backled_timer_started = 1;
}


static void ledanimation_timer_function(unsigned long data)
{
	unsigned char * anim;
	if(anim_current == NULL){
		return;
	}
	if(leddrv_func == NULL){
		return;
	}

#ifdef DEBUG
	printk(KERN_ERR "[KHJ] led animation [%s][remain:%u][frame:%u/%u]\n",
			anim_current->name,
			anim_repeat_count,
			anim_frame_count + 1,
			anim_total_frames);
	//printk(KERN_ERR "[KHJ] %p, %p\n", leddrv_ops, leddrv);
#endif

	// ctrl led on/off
	anim = anim_current->anim + (leddrv_func->num_leds*anim_frame_count);
	_draw_led_frame(anim);

	anim_frame_count++;
	if(anim_frame_count >= anim_total_frames){
		anim_frame_count = 0;

		// TODO: fix 
		if(strcmp(anim_current->name, "__boot__") == 0){
			anim_frame_count = 16;
		}
		if(anim_repeat_count > 0 && --anim_repeat_count <= 0){
			delete_anim_current();
#ifdef DEBUG
			printk(KERN_ERR "[KHJ] end animation\n");
#endif			
			return;
		}		
	} 
	mod_timer(&anim_timer, jiffies + msecs_to_jiffies(anim_current->interval));
}

static struct ledanim_t * copy_ledanim_data_from_user(unsigned long arg)
{
	struct ledanim_t * ledanim;
	unsigned char * anim_array;

	ledanim = (struct ledanim_t *)kmalloc(sizeof(struct ledanim_t), GFP_KERNEL);
	if(!ledanim){
		return NULL;
	}
	memset(ledanim, 0, sizeof(struct ledanim_t));

	if (copy_from_user(ledanim, (struct ledanim_t __user *)arg, sizeof(struct ledanim_t))){
		kfree(ledanim);
		printk(KERN_ERR "%s: copy_from_user() failed\n", __func__);
		return NULL;
	}

	// copy animation array
	if(ledanim->anim_count <= 0){
		kfree(ledanim);
		printk(KERN_ERR "%s: invalid animation count: %lu\n", __func__, ledanim->anim_count);
		return NULL;
	}
	
	anim_array = (unsigned char*)kmalloc(sizeof(unsigned char) * ledanim->anim_count, GFP_KERNEL);
	if(!anim_array){
		kfree(ledanim);
		return NULL;
	}
	memset(anim_array, 0, sizeof(unsigned char) * ledanim->anim_count);

	if (copy_from_user(anim_array, ledanim->anim, sizeof(unsigned char) * ledanim->anim_count)){
		kfree(anim_array);
		kfree(ledanim);
		printk(KERN_ERR "%s: copy_from_user() failed -- anim array\n", __func__);
		return NULL;
	}

	ledanim->anim = anim_array;

	return ledanim;
}

static int ledanim_ioctl_get_brightness(unsigned long arg)
{
	int i;
	int index_max;
	struct led_brightness_t led_brightness;

	if(!leddrv_func){
		return -EFAULT;
	}

	index_max = leddrv_func->num_leds/3;
	if(backled_leddrv_func != NULL){
		index_max += backled_leddrv_func->num_leds/3;
	}

	if (copy_from_user(&led_brightness, (int __user *)arg, sizeof(struct led_brightness_t))){
		printk(KERN_ERR "%s: copy_from_user() failed\n", __func__);
		return -EFAULT;
	}

	if(led_brightness.index < 0 || led_brightness.index >= index_max){
		printk(KERN_ERR "%s: invalid index: %d\n", __func__, led_brightness.index);
		return -EFAULT;
	}
	
	i = led_brightness.index * 3;
	if(_get_led(i, &led_brightness.red) != 0 ||
		_get_led(i + 1, &led_brightness.green) != 0 ||
		_get_led(i + 2, &led_brightness.blue) != 0){
		printk(KERN_ERR "%s: _get_led() failed\n", __func__);
		return -EFAULT;
	}

	if (copy_to_user((void __user *)arg, &led_brightness, sizeof(struct led_brightness_t))){
		printk(KERN_ERR "%s: copy_to_user() failed\n", __func__);
		return -EFAULT;
	}

	return 0;	
}

static int ledanim_ioctl_set_brightness(unsigned long arg)
{
	int i;
	int index_max;
	struct led_brightness_t led_brightness;

	if(!leddrv_func){
		return -EFAULT;
	}

	index_max = leddrv_func->num_leds/3;
	if(backled_leddrv_func != NULL){
		index_max += backled_leddrv_func->num_leds/3;
	}

	if (copy_from_user(&led_brightness, (int __user *)arg, sizeof(struct led_brightness_t))){
		printk(KERN_ERR "%s: copy_from_user() failed\n", __func__);
		return -EFAULT;
	}

	if(led_brightness.index < 0 || led_brightness.index >= index_max){
		printk(KERN_ERR "%s: invalid index: %d\n", __func__, led_brightness.index);
		return -EFAULT;
	}

	if(led_brightness.index < leddrv_func->num_leds/3) {
		if(anim_current){
			delete_anim_current();
			del_timer(&anim_timer);
			_set_led_all_off();
		}
	} else {
		if(is_backled_timer_started){
			del_timer(&backled_timer);
			is_backled_timer_started = 0;
		}
	}

	i = led_brightness.index * 3;
	if(_set_led(i, led_brightness.red) != 0 ||
		_set_led(i + 1, led_brightness.green) != 0 ||
		_set_led(i + 2, led_brightness.blue) != 0){
		printk(KERN_ERR "%s: _set_led() failed\n", __func__);
		return -EFAULT;
	}

	if(led_brightness.index < leddrv_func->num_leds/3) {
		_update_led();
	} else {
		backled_leddrv_func->update(backled_leddrv_func->dev);
	}

	return 0;
}

static int ledanim_ioctl_start(unsigned long arg)
{
	struct ledanim_t *	ledanim;

	if(!leddrv_func){
		return -EFAULT;
	}

	ledanim = copy_ledanim_data_from_user(arg);
	if(!ledanim){
		return -EFAULT;
	}

	if(anim_current){
		delete_anim_current();
		del_timer(&anim_timer);
		_set_led_all_off();
	}
	start_anim_current(ledanim);
	return 0;
}

static int ledanim_ioctl_stop(unsigned long arg)
{
	if(!leddrv_func){
		return -EFAULT;
	}

	if(anim_current){
		del_timer(&anim_timer);
		delete_anim_current();
	}
	_set_led_all_off();
	return 0;
}

static int ledanim_ioctl_get_led_count(unsigned long arg)
{
	int num_leds;

	if(!leddrv_func){
		return -EFAULT;
	}

	num_leds = leddrv_func->num_leds;
	if(backled_leddrv_func != NULL){
		num_leds += backled_leddrv_func->num_leds;
	}

	if (copy_to_user((void __user *)arg, &num_leds, sizeof(int))){
		printk(KERN_ERR "%s: copy_to_user() failed\n", __func__);
		return -EFAULT;
	}
	return 0;
}

static int ledanim_ioctl_set_frame(unsigned long arg) 
{
	struct ledanim_t *	ledanim;

	if(!leddrv_func){
		return -EFAULT;
	}

	ledanim = copy_ledanim_data_from_user(arg);
	if(!ledanim){
		return -EFAULT;
	}

	if(anim_current){
		delete_anim_current();
		del_timer(&anim_timer);
	}

#ifdef DEBUG
	if(1) {
		int i;
		printk(KERN_INFO "LED set frame:\n");
		for(i = 0; i < leddrv_func->num_leds ; i+=3){
			printk(KERN_INFO "    [%d] 0x%02x%02x%02x\n", i/3,
				ledanim->anim[i+0],
				ledanim->anim[i+1],
				ledanim->anim[i+2]);
		}
	}
#endif

	_draw_led_frame(ledanim->anim);

	if(backled_leddrv_func != NULL){
		if(ledanim->anim_count > leddrv_func->num_leds){
			int i;

			if(ledanim->anim_count >= backled_leddrv_func->num_leds + leddrv_func->num_leds){
				if(is_backled_timer_started){
					del_timer(&backled_timer);
					is_backled_timer_started = 0;
				}
				for(i = 0; i < backled_leddrv_func->num_leds; i++){
					_set_led(leddrv_func->num_leds + i, ledanim->anim[leddrv_func->num_leds + i]);
				}
				backled_leddrv_func->update(backled_leddrv_func->dev);
			}
		}
	}

	kfree(ledanim->anim);
	kfree(ledanim);	

	return 0;
}

static long ledanim_unlocked_ioctl(struct file *file, unsigned int req, unsigned long arg)
{
	int err = -EBADRQC;
    int prio = LED_DEFAULT;
//struct led_priority_t *led_p = NULL;
struct led_priority_t *led_p = file->private_data;
//led_p = kzalloc(sizeof(struct led_priority_t), GFP_KERNEL);

#ifdef DEBUG
	printk("%s: ioctl: req 0x%x\n", __func__, req);
#endif

	switch (req) {
	case LED_GET_BRIGHTNESS:
		err = ledanim_ioctl_get_brightness(arg);
		break;
	case LED_SET_BRIGHTNESS:
		err = ledanim_ioctl_set_brightness(arg);
		break;
	case LED_SET_FRAME:
        if(g_priority <= led_p->priority){
		    err = ledanim_ioctl_set_frame(arg);
            g_priority = led_p->priority;
        }
		break;
	case LED_ANIM_START:
        if(g_priority <= led_p->priority){
		    err = ledanim_ioctl_start(arg);
            g_priority = led_p->priority;
        }
		break;
	case LED_ANIM_STOP:
		err = ledanim_ioctl_stop(arg);
        g_priority = LED_DEFAULT;
		break;
	case LED_GET_COUNT:
		err = ledanim_ioctl_get_led_count(arg);
		break;
    case LED_SET_PRIORITY:
        prio = (int)arg;

#ifdef DEBUG
		printk(KERN_ERR "LED_SET_PRIORITY %d\n", prio);
#endif
        if(prio == LED_INIT) {
            led_p->priority = LED_DEFAULT;
            g_priority = LED_DEFAULT;
        }else{
            led_p->priority = prio; 
        }
        file->private_data = led_p;
        err = 0;
        break;
	}
	return err;
}
static int ledanim_open(struct inode *inode, struct file *file) {
	struct led_priority_t *led_p = NULL;
	led_p = kzalloc(sizeof(struct led_priority_t), GFP_KERNEL);
	led_p->priority = LED_DEFAULT;
	file->private_data = led_p;
	return 0;
}

static int ledanim_release(struct inode *inode, struct file *file) {

	struct led_priority_t *led_p = file->private_data;
	file->private_data = NULL;
	kfree(led_p);
	return 0;
}

static const struct file_operations ledanim_fops = {
	.owner		= THIS_MODULE,	
	.llseek		= noop_llseek,
	.open		= ledanim_open,
	.unlocked_ioctl	= ledanim_unlocked_ioctl,
	.release	= ledanim_release,
};

static struct miscdevice ledanim_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "ledanim",
	.fops	= &ledanim_fops,
};

static int __init ledanim_init(void)
{
#ifdef DEBUG
	printk("%s: init\n", __func__);
#endif

	return misc_register(&ledanim_miscdev);
}
device_initcall(ledanim_init);



int register_leddrv_func(struct leddrv_func * func)
{
	leddrv_func = func;

	init_timer(&anim_timer);
	anim_timer.function = ledanimation_timer_function;

	start_anim_current(&ledanim_boot);
	return 0;
}
EXPORT_SYMBOL(register_leddrv_func);

int register_backled_leddrv_func(struct leddrv_func * func)
{
	if(backled_leddrv_func){
		return -1;
	}

	backled_leddrv_func = func;
	start_backled_boot_animation();

	return 0;
}
EXPORT_SYMBOL(register_backled_leddrv_func);


int ledanim_set_max_current(unsigned char value)
{
	if(!leddrv_func){
		printk(KERN_ERR "%s: no led driver", __func__);
		return -EFAULT;
	}	
	return leddrv_func->set_max_current(leddrv_func->dev, value);
}
EXPORT_SYMBOL(ledanim_set_max_current);

int ledanim_get_max_current(unsigned char * value) 
{
	if(!leddrv_func){
		printk(KERN_ERR "%s: no led driver", __func__);
		return -EFAULT;
	}	
	return leddrv_func->get_max_current(leddrv_func->dev, value);
}
EXPORT_SYMBOL(ledanim_get_max_current);

