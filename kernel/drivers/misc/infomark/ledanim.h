#ifndef __LEDANIM_H__
#define __LEDANIM_H__


#define LED_NAME_LEN	20

struct ledanim_t {
	char name[LED_NAME_LEN];
	short interval;
	unsigned long anim_count;
	unsigned char * anim;
	unsigned long repeat;
};

struct led_brightness_t {
	int index;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};


struct leddrv_func {
	int num_leds;
	struct device *dev;
	int (*get_led_brightness)(struct device *dev, int index, unsigned char * value);
	int (*set_led_brightness)(struct device *dev, int index, unsigned char value);
	int (*update)(struct device *dev);
	int (*power_enable)(struct device *dev, int enable);
	int (*get_max_current)(struct device *dev, unsigned char * value);
	int (*set_max_current)(struct device *dev, unsigned char value);
};

int register_leddrv_func(struct leddrv_func * func);
int register_backled_leddrv_func(struct leddrv_func * func);

struct led_priority_t {
    int priority;
};

#define LEDANIM_IOCTL_NUM	'L'

#define LED_GET_BRIGHTNESS	_IOWR(LEDANIM_IOCTL_NUM, 1, void *)
#define LED_SET_BRIGHTNESS	_IOWR(LEDANIM_IOCTL_NUM, 2, void *)
#define LED_ANIM_START		_IOWR(LEDANIM_IOCTL_NUM, 3, void *)
#define LED_ANIM_STOP		_IOWR(LEDANIM_IOCTL_NUM, 4, void *)
#define LED_GET_COUNT		_IOWR(LEDANIM_IOCTL_NUM, 5, void *)
#define LED_SET_FRAME		_IOWR(LEDANIM_IOCTL_NUM, 6, void *)
#define LED_SET_PRIORITY	_IOWR(LEDANIM_IOCTL_NUM, 7, void *)

/* [START] 2017.08.21 kkkim Led*/
#define LED_EMER    100
#define LED_HIGHEST 90
#define LED_HIGH    70
#define LED_NORMAL  50
#define LED_LOW     30
#define LED_LOWEST  10
#define LED_DEFAULT 0
#define LED_INIT    255
/* [END] 2017.08.21 kkkim Led*/

#endif
